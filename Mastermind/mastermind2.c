#define pr_fmt(fmt) "mastermind2: " fmt
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/vmalloc.h>
#include <linux/random.h>
#include <linux/time.h>
#include <linux/moduleparam.h>

#define NUM_PEGS 4
int num_games;
int changed;
int inval;

struct mm_game {
  /** true if user is in the middle of a game */
	bool game_active;

  /** code that player is trying to guess */
	int target_code[NUM_PEGS];

  /** tracks number of guesses user has made */
	unsigned num_guesses;

  /** current status of the game */
	char game_status[80];

  /** buffer that logs guesses for the current game */
	char *user_view;

	kuid_t id;

	struct timespec start;

	struct timespec end;

	struct list_head next;
};

LIST_HEAD(game);
/** the number of colors possible */
int num_colors;

/** spinlock variable **/
static spinlock_t lock;

/** tells the module whether to randomize starting code or go with default*/
static int rand = 0;
module_param(rand, int, 0644);
MODULE_PARM_DESC(rand, "whether to randomize starting code or not.");

/* converts a character to its int value if the character represents an int */
static int ctoi(char c)
{
	return (int)(c - 48);
}

struct mm_game *find_game(kuid_t id)
{
	struct mm_game *gm;
	struct list_head *here, *next;
	list_for_each_safe(here, next, &game) {
		gm = list_entry(here, struct mm_game, next);
		if (uid_eq(current_uid(), gm->id)) {
			return gm;
		}
	}

	gm = kzalloc(sizeof(struct mm_game), GFP_USER);
	if (!gm) {
		return NULL;
	}
	INIT_LIST_HEAD(&gm->next);
	list_add(&gm->next, &game);
	num_games++;
	snprintf(gm->game_status, 80, "No game yet\n");
	gm->id = current_uid();
	getnstimeofday(&gm->start);
	getnstimeofday(&gm->end);
	gm->end.tv_nsec = -1;
	gm->user_view = vmalloc(PAGE_SIZE);
	if (!gm->user_view) {
		kfree(gm);
		return NULL;
	}
	return gm;
}

/* checks if the given int is in the int array */
static bool seen(int i, int *ar, int count)
{
	int x;
	if (count == 0) {
		return false;
	}
	for (x = 0; x < count; x++) {
		if (i == ar[x]) {
			return true;
		}
	}
	return false;
}

/* returns the signed equivalent of the lesser of two unsigned size_t values */
static size_t mymin(size_t a, size_t b)
{
	if (a <= b) {
		return (ssize_t) a;
	}
	return (ssize_t) b;
}

/**
 * mm_read() - callback invoked when a process reads from
 * /dev/mm
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: destination buffer to store output
 * @count: number of bytes in @ubuf
 * @ppos: file offset (in/out parameter)
 *
 * Write to @ubuf the current status of the game, offset by
 * @ppos. Copy the lesser of @count and (string length of @game_status
 * - *@ppos). Then increment the value pointed to by @ppos by the
 * number of bytes copied. If @ppos is greater than or equal to the
 * length of @game_status, then copy nothing.
 *
 * Return: number of bytes written to @ubuf, or negative on error
 */
static ssize_t mm_read(struct file *filp, char __user * ubuf, size_t count,
		       loff_t * ppos)
{
	size_t pos;
	ssize_t size;
	int ret;
	struct mm_game *gm;
	spin_lock(&lock);
	gm = find_game(current_uid());
	if (!gm) {
		spin_unlock(&lock);
		return -ENOMEM;
	}
	if (strlen(gm->game_status) > *ppos) {
		pos = strlen(gm->game_status) - *ppos;
		size = mymin(count, pos);
	}

	else {
		spin_unlock(&lock);
		return 0;
	}
	ret = copy_to_user(ubuf, gm->game_status + *ppos, size);
	if (ret != 0) {
		spin_unlock(&lock);
		return -EPERM;
	}
	*ppos += size;
	spin_unlock(&lock);
	return size;

}

/**
 * mm_write() - callback invoked when a process writes to /dev/mm
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * If the user is not currently playing a game, then return -EINVAL.
 *
 * If @count is less than NUM_PEGS, then return -EINVAL. Otherwise,
 * interpret the first NUM_PEGS characters in @ubuf as the user's
 * guess. For each guessed peg, calculate how many are in the correct
 * value and position, and how many are simply the correct value. Then
 * update @num_guesses, @game_status, and @user_view.
 *
 * <em>Caution: @ubuf is NOT a string; it is not necessarily
 * null-terminated.</em> You CANNOT use strcpy() or strlen() on it!
 *
 * Return: @count, or negative on error
 */
static ssize_t mm_write(struct file *filp, const char __user * ubuf,
			size_t count, loff_t * ppos)
{
	char buf[80];
	int buf2[4];
	char buf3[30];
	int ret, x, i, cur, black = 0, white = 0;
	size_t size;
	int prev[NUM_PEGS];
	struct mm_game *gm;
	spin_lock(&lock);
	gm = find_game(current_uid());
	if (!gm) {
		spin_unlock(&lock);
		return -ENOMEM;
	}
	if (gm->game_active == false) {
		spin_unlock(&lock);
		return -EPERM;
	}
	size = mymin(count, sizeof(buf));
	ret = copy_from_user(buf, ubuf, size);
	if (ret < 0) {
		spin_unlock(&lock);
		return -EINVAL;
	}
	/*takes the first 4 inputs, converts them to ints and generates black and white pegs */
	for (x = 0; x < NUM_PEGS; x++) {
		cur = ctoi(buf[x]);
		buf2[x] = cur;
		if (cur >= 6 || cur < 0) {
			spin_unlock(&lock);
			return -EINVAL;
		}
		for (i = 0; i < NUM_PEGS; i++) {
			if (cur == gm->target_code[i] && x == i) {
				black++;
				prev[x] = cur;
				break;
			}
			if (cur == gm->target_code[i] && !seen(cur, prev, x)) {
				white++;
				prev[x] = cur;
				continue;
			}
		}
	}
	gm->num_guesses++;
	memset(gm->game_status, 0, 80);
	snprintf(gm->game_status, 80,
		 "Guess %d: %d black peg(s), %d white peg(s)\n",
		 gm->num_guesses, black, white);
	snprintf(buf3, 30, "Guess %d: %d%d%d%d  |  B%d W%d\n", gm->num_guesses,
		 buf2[0], buf2[1], buf2[2], buf2[3], black, white);
	strcat(gm->user_view, buf3);
	i = 1;
	for (x = 0; x < NUM_PEGS; x++) {
		if (gm->target_code[x] != buf2[x]) {
			i = 0;
			break;
		}
	}
	if (i == 1) {
		getnstimeofday(&gm->end);
		snprintf(gm->game_status, 80, "Correct! Game Over.\n");
		snprintf(buf3, 30, "Correctly guessed: %d%d%d%d\n",
			 buf2[0], buf2[1], buf2[2], buf2[3]);
		strcat(gm->user_view, buf3);

		gm->game_active = false;
	}
	spin_unlock(&lock);
	return size;
}

/**
 * mm_ctl_write() - callback invoked when a process writes to
 * /dev/mm_ctl
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * Copy the contents of @ubuf, up to the lesser of @count and 8 bytes,
 * to a temporary location. Then parse that character array as
 * following:
 *
 *  start - Start a new game. If a game was already in progress, restart it.
 *  quit  - Quit the current game. If no game was in progress, do nothing.
 *
 * If the input is none of the above, then return -EINVAL.
 *
 * Return: @count, or negative on error
 */
static ssize_t mm_ctl_write(struct file *filp, const char __user * ubuf,
			    size_t count, loff_t * ppos)
{
	char buf[8];
	size_t max = 8;
	size_t size = mymin(count, max);
	int ret, x;
	unsigned int code[] = { 0, 0, 1, 2 };
	struct mm_game *gm;
	spin_lock(&lock);
	gm = find_game(current_uid());
	if (!gm) {
		spin_unlock(&lock);
		return -ENOMEM;
	}
	if (rand == 1) {
		for (x = 0; x < NUM_PEGS; x++) {
			get_random_bytes(&code[x], sizeof(code[x]));
			code[x] = code[x] % num_colors;
		}
	}
	if (size < 4 || size > 9) {
		spin_unlock(&lock);
		return -EINVAL;
	}
	/*sets the character after "start" and "quit to null
	   if both are overwritten by a non-null, it has to be an invalid input" */
	buf[5] = '\0';
	buf[4] = '\0';
	ret = copy_from_user(buf, ubuf, size);
	if (ret < 0) {
		spin_unlock(&lock);
		return -EINVAL;
	}
	buf[6] = '\0';
	if (strcmp(buf, "colors") == 0) {
		if (ctoi(buf[7]) < 2 || ctoi(buf[7]) > 9) {
			spin_unlock(&lock);
			return -EINVAL;
		}
		if (!capable(CAP_SYS_ADMIN)) {
			spin_unlock(&lock);
			return -EPERM;
		}
		num_colors = ctoi(buf[7]);
		spin_unlock(&lock);
		return count;
	}
	if (buf[5] != '\0') {
		spin_unlock(&lock);
		return -EINVAL;
	}
	buf[5] = '\0';
	/*if the input is "start" it starts the game and initializes variables */
	if (strcmp(buf, "start") == 0) {
		gm->game_active = true;
		for (x = 0; x < NUM_PEGS; x++) {
			gm->target_code[x] = code[x];
		}
		gm->num_guesses = 0;
		memset(gm->user_view, 0, PAGE_SIZE);
		snprintf(gm->game_status, 80, "Starting game\n");
		spin_unlock(&lock);
		return count;
	} else if (buf[4] != '\0') {
		spin_unlock(&lock);
		return -EINVAL;
	} else {
		buf[4] = '\0';
		if (strcmp(buf, "quit") == 0) {
			gm->game_active = false;
			snprintf(gm->game_status, 80,
				 "Game over. The code was %d%d%d%d.\n",
				 gm->target_code[0], gm->target_code[1],
				 gm->target_code[2], gm->target_code[3]);
			spin_unlock(&lock);
			return count;
		}
	}
	spin_unlock(&lock);
	return -EINVAL;
}

static const struct file_operations f_ops = {
	.read = mm_read,
	.write = mm_write,
	.mmap = mm_mmap,
};

static struct miscdevice m_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mm",
	.fops = &f_ops,
	.mode = 0666,
};

static const struct file_operations f_ctl_ops = {
	.write = mm_ctl_write,
};

static struct miscdevice m_ctl_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mm_ctl",
	.fops = &f_ctl_ops,
	.mode = 0666,
};

/**
 * cs421net_top() - top-half of CS421Net ISR
 * @irq: IRQ that was invoked (ignored)
 * @cookie: Pointer to data that was passed into
 * request_threaded_irq() (ignored)
 *
 * If @irq is CS421NET_IRQ, then wake up the bottom-half. Otherwise,
 * return IRQ_NONE.
 */
static irqreturn_t cs421net_top(int irq, void *cookie)
{
	if (irq == CS421NET_IRQ) {
		return IRQ_WAKE_THREAD;
	}
	return IRQ_NONE;
}

/**
 * cs421net_bottom() - bottom-half to CS421Net ISR
 * @irq: IRQ that was invoked (ignore)
 * @cookie: Pointer that was passed into request_threaded_irq()
 * (ignored)
 *
 * Fetch the incoming packet, via cs421net_get_data(). If:
 *   1. The packet length is exactly equal to the number of digits in
 *      the target code, and
 *   2. If all characters in the packet are valid ASCII representation
 *      of valid digits in the code, then
 * Set the target code to the new code, and increment the number of
 * types the code was changed remotely. Otherwise, ignore the packet
 * and increment the number of invalid change attempts.
 *
 * Return: always IRQ_HANDLED
 */
static irqreturn_t cs421net_bottom(int irq, void *cookie)
{
	char const *dat;
	size_t len;
	int x, z;
	struct mm_game *gm;
	struct list_head *here;
	spin_lock(&lock);
	printk(KERN_ALERT "here\n");
	dat = cs421net_get_data(&len);
	if (dat == NULL || len != NUM_PEGS) {
		inval++;
		spin_unlock(&lock);
		return IRQ_HANDLED;
	}
	for (x = 0; x < NUM_PEGS; x++) {
		z = ctoi(dat[x]);
		if (z < 0 || z > num_colors) {
			inval++;
			spin_unlock(&lock);
			return IRQ_HANDLED;
		}
	}
	list_for_each(here, &game) {
		gm = list_entry(here, struct mm_game, next);
		for (x = 0; x < NUM_PEGS; x++) {
			z = ctoi(dat[x]);
			gm->target_code[x] = z;
		}
	}
	changed++;
	spin_unlock(&lock);
	return IRQ_HANDLED;
}

/**
 * mm_stats_show() - callback invoked when a process reads from
 * /sys/devices/platform/mastermind/stats
 *
 * @dev: device driver data for sysfs entry (ignored)
 * @attr: sysfs entry context (ignored)
 * @buf: destination to store game statistics
 *
 * Write to @buf, up to PAGE_SIZE characters, a human-readable message
 * containing these game statistics:
 *   - Number of pegs (digits in target code)
 *   - Number of colors (range of digits in target code)
 *   - Number of valid network messages (see Part 4)
 *   - Number of invalid network messages (see Part 4)
 *   - Number of active players (see Part 5)
 *
 * @return Number of bytes written to @buf, or negative on error.
 */
static ssize_t mm_stats_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int ret;
	char buf2[256];
	struct mm_game *gm;
	struct list_head *here;
	struct timespec best_t2 = {
		.tv_sec = 0,
		.tv_nsec = -1
	};
	struct timespec *best_t = &best_t2;
	kuid_t best_id;
	spin_lock(&lock);
	best_id.val = 0;
	list_for_each(here, &game) {
		gm = list_entry(here, struct mm_game, next);
		if (gm->end.tv_nsec == -1) {
			continue;
		}
		if (best_t->tv_nsec == -1) {
			best_t->tv_sec = (gm->end.tv_sec - gm->start.tv_sec);
			best_t->tv_nsec = (gm->end.tv_nsec - gm->start.tv_nsec);
			best_id = gm->id;
			continue;
		}
		if ((gm->end.tv_sec - gm->start.tv_sec) < best_t->tv_sec) {
			best_t->tv_sec = (gm->end.tv_sec - gm->start.tv_sec);
			best_t->tv_nsec = (gm->end.tv_nsec - gm->start.tv_nsec);
			best_id = gm->id;
			continue;
		}
		if ((gm->end.tv_sec - gm->start.tv_sec) == best_t->tv_sec) {
			if ((gm->end.tv_nsec - gm->start.tv_nsec) <
			    best_t->tv_nsec) {
				best_t->tv_sec =
				    (gm->end.tv_sec - gm->start.tv_sec);
				best_t->tv_nsec =
				    (gm->end.tv_nsec - gm->start.tv_nsec);
				best_id = gm->id;
				continue;
			}
		}
	}
	if (best_t->tv_nsec == -1) {
		ret =
		    scnprintf(buf, PAGE_SIZE,
			      "CS421 Mastermind Stats\nNumber of pegs: %d\nNumber of colors: %d\nNumber of times code was changed: %d\nNumber of invalid code change attempts: %d\nNumber of games started: %d\nNo game completions yet\n",
			      NUM_PEGS, num_colors, changed, inval, num_games);
		if (ret == 0) {
			spin_unlock(&lock);
			return -ENOMEM;
		}
		spin_unlock(&lock);
		return ret;
	}

	ret =
	    scnprintf(buf, PAGE_SIZE,
		      "CS421 Mastermind Stats\nNumber of pegs: %d\nNumber of colors: %d\nNumber of times code was changed: %d\nNumber of invalid code change attempts: %d\nNumber of games started: %d\nBest time was UID %d with a time of %ld seconds and %ld nanoseconds\n",
		      NUM_PEGS, num_colors, changed, inval, num_games,
		      (int)best_id.val, (long)best_t->tv_sec,
		      (long)best_t->tv_nsec);
	if (ret == 0) {
		spin_unlock(&lock);
		return -ENOMEM;
	}
	spin_unlock(&lock);
	return ret;
}

static DEVICE_ATTR(stats, S_IRUGO, mm_stats_show, NULL);

/**
 * mastermind_probe() - callback invoked when this driver is probed
 * @pdev platform device driver data
 *
 * Return: 0 on successful probing, negative on error
 */
static int mastermind_probe(struct platform_device *pdev)
{
	int retval;
	spin_lock(&lock);
	retval = device_create_file(&pdev->dev, &dev_attr_stats);
	if (retval) {
		pr_err("Could not create sysfs entry\n");
		return retval;
	}
	pr_info("Initializing the game.\n");
	if (misc_register(&m_device) < 0) {
		goto free1;
	}
	if (misc_register(&m_ctl_device) < 0) {
		goto free2;
	}
	if (request_threaded_irq
	    (CS421NET_IRQ, cs421net_top, cs421net_bottom, 0, "CS421",
	     NULL) < 0) {
		goto free3;
	}
	cs421net_enable();

	num_colors = 6;
	num_games = 0;
	inval = 0;
	changed = 0;
	spin_unlock(&lock);
	return 0;

free3:
	misc_deregister(&m_ctl_device);
free2:
	misc_deregister(&m_device);
free1:
	device_remove_file(&pdev->dev, &dev_attr_stats);
	spin_unlock(&lock);
	return -ENOMEM;
}

/**
 * mastermind_remove() - callback when this driver is removed
 * @pdev platform device driver data
 *
 * Return: Always 0
 */
static int mastermind_remove(struct platform_device *pdev)
{
	struct list_head *pos, *here;
	struct mm_game *gm;
	pr_info("Freeing resources.\n");
	device_remove_file(&pdev->dev, &dev_attr_stats);
	list_for_each_safe(pos, here, &game) {
		gm = list_entry(pos, struct mm_game, next);
		list_del(pos);
		if (gm != NULL) {
			if (gm->user_view != NULL) {
				vfree(gm->user_view);
			}
			kfree(gm);
		}
	}
	misc_deregister(&m_device);
	misc_deregister(&m_ctl_device);
	free_irq(CS421NET_IRQ, NULL);
	cs421net_disable();
	return 0;
}

static struct platform_driver cs421_driver = {
	.driver = {
		   .name = "mastermind",
		   },
	.probe = mastermind_probe,
	.remove = mastermind_remove,
};

static struct platform_device *pdev;

static int __init cs421_init(void)
{
	pdev = platform_device_register_simple("mastermind", -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);
	return platform_driver_register(&cs421_driver);
}

/**
 * cs421_exit() - remove the platform driver
 * Unregister the driver from the platform bus.
 *
 */
static void __exit cs421_exit(void)
{
	platform_driver_unregister(&cs421_driver);
	platform_device_unregister(pdev);
}

module_init(cs421_init);
module_exit(cs421_exit);

MODULE_DESCRIPTION("CS421 Mastermind Game++");
MODULE_LICENSE("GPL");
