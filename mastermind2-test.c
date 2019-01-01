/* https://www.linuxquestions.org/questions/programming-9/mmap-tutorial-c-c-511265/

https://www.gnu.org/software/libc/manual/html_node/Setuid-Program-Example.html
*/
/*
 *NOTE TO GRADER:
 to run regular tests, first run program as regualar user, then again with sudo with exactly ONE (not including program name) command line argument (can be anything)

 for extra credit :

 remove the module after running regular tests and reinsert for fresh games
 run twice, both with exactly TWO (not including program name) command line arguments (can be anything), once as regular user, then as sudo.

 All together, running all of it looks like this:

 make
 sudo insmod mastermind2.ko
 ./mastermind2-test
 sudo ./mastermind2-test 1
 sudo rmmod mastermind2
 sudo insmod mastermind2.ko
 ./mastermind2-test 1 2
 sudo ./mastermind2-test 1 2
 
 
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <linux/capability.h>
//#include <linux/uaccess.h>
//#include <linux/uidgid.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/user.h>
#include <string.h>
#include "cs421net.h"

unsigned test_passed;
unsigned test_failed;

#define CHECK_IS_EQUAL(valA,valB) \
	do { \
		if ((valA) == (valB)) { \
			test_passed++; \
		} else { \
			test_failed++; \
			printf("%d: FAIL\n", __LINE__); \
		} \
	} while(0);

#define CHECK_IS_NEGATIVE(val)			\
  do{ \
    if ((val < 0)){ \
      test_passed++; \
    } else{ \
      test_failed++; \
      printf("%d: FAIL\n", __LINE__); \
    }\
  } while(0);


int main(int argc, char **argv) {
  test_passed = 0;
  test_failed = 0;
  char buf[256];
  int ret;
  unsigned prev;
  int mm_ctl;
  FILE *mm, *stats;
  void* map;
  int mm2;
  if (argc == 2){
    printf("Starting test 8: change colors for root user\n");
    mm_ctl = open("/dev/mm_ctl", O_RDWR);
    mm2 = open("/dev/mm", O_RDWR);
    if (mm_ctl < 1 || mm2 < 1){
      printf("Opening failed\n");
      exit(1);
    }
    ret = write(mm_ctl, "start", 6);
    stats = fopen("/sys/devices/platform/mastermind/stats", "r");
    if (stats == NULL){
      printf("Opening failed\n");
      exit(1);
    }

    ret = write(mm_ctl, "colors 4", 9);
    if (fread(buf, 1, 256, stats) <= 0){
      printf("Read failed\n");
    }
    buf[166] = '\0';
    //check that color was changed properly if the user is root
    CHECK_IS_EQUAL(strcmp("CS421 Mastermind Stats\nNumber of pegs: 4\nNumber of colors: 4\nNumber of times code was changed: 1\nNumber of invalid code change attempts: 2\nNumber of games started: 2\n", buf), 0);
    printf("Starting test 12: multiple games\n");

    close(mm_ctl);
    fclose(stats);
    close(mm2);
    mm_ctl = open("/dev/mm_ctl", O_RDWR);
    mm2 = open("/dev/mm", O_RDWR);
    stats = fopen("/sys/devices/platform/mastermind/stats", "r");
    cs421net_init();
    ret = write(mm_ctl, "start", 6);
    memset(buf, 0, 256);
    ret = read(mm2, buf, 256); 
    if (ret <= 0){
      printf("read failed\n");
    }
    //check that this game is not yet completed
    CHECK_IS_EQUAL(strcmp("Starting game\n", buf), 0);
    if (!cs421net_send("1111", 4)){
      printf("Network send failed\n");
      exit(1);
    }
    sleep(1);
    ret = write(mm2, "1111", 4);
    if (ret <= 0){
      printf("write failed\n");
    }
    close(mm2);
    mm2 = open("/dev/mm", O_RDWR);
    memset(buf, 0, 256);
    ret = read(mm2, buf, 256); 
    if (ret <= 0){
      printf("read failed\n");
    }
    //checks that the guess 1111 was correct
    CHECK_IS_EQUAL(strcmp("Correct! Game Over.\n", buf), 0);
    
    close(mm_ctl);
    fclose(stats);
    close(mm2);
    printf("Sudo Run: %u Tests passed, %u Tests failed\n", test_passed, test_failed);
    exit(0);
  }
  if (argc == 3){
    printf("Starting test 14: extra credit\n");
    mm_ctl = open("/dev/mm_ctl", O_RDWR);
    mm2 = open("/dev/mm", O_RDWR);
    stats = fopen("/sys/devices/platform/mastermind/stats", "r");
    ret = write(mm_ctl, "start", 6);
    if (getuid() != 0){
      sleep(3);
      ret = write(mm2, "0012", 4);
      memset(buf, 0, 256);
      ret = read(mm2, buf, 256); 
      if (ret <= 0){
	printf("read failed\n");
      }
      //check that game was sucessfully won
      CHECK_IS_EQUAL(strcmp(buf, "Correct! Game Over.\n"), 0);
      if (fread(buf, 1, 256, stats) <= 0){
	printf("Read failed\n");
      }
      buf[213] = '\0';
      //check that the time is right
      CHECK_IS_EQUAL(buf[204], '3');
      printf("%s\n", buf);
      
    }
    if(getuid() == 0){
      sleep(1);
      ret = write(mm2, "0012", 4);
      memset(buf, 0, 256);
      ret = read(mm2, buf, 256); 
      if (ret <= 0){
	printf("read failed\n");
      }
      //check that game was sucessfully won
      CHECK_IS_EQUAL(strcmp(buf, "Correct! Game Over.\n"), 0);
      if (fread(buf, 1, 256, stats) <= 0){
	printf("Read failed\n");
      }
      buf[202] = '\0';
      //check that the time beat the last one
      CHECK_IS_EQUAL(strcmp("CS421 Mastermind Stats\nNumber of pegs: 4\nNumber of colors: 6\nNumber of times code was changed: 0\nNumber of invalid code change attempts: 0\nNumber of games started: 2\nBest time was UID 0 with a time of 1", buf), 0);
      buf[202] = ' ';
      buf[211] = '\0';
      printf("%s\n", buf);
      
    }
    close(mm_ctl);
    fclose(stats);
    close(mm2);
    printf("Extra Credit: %u Tests passed, %u Tests failed\n", test_passed, test_failed);
    exit(1);
  }
  
  mm_ctl = open("/dev/mm_ctl", O_RDWR);
  mm2 = open("/dev/mm", O_RDWR);
  if (mm_ctl < 1 || mm2 < 1){
    printf("Opening failed\n");
    exit(1);
  }
  mm = fopen("/dev/mm", "r+");
   if (mm == NULL){
    printf("Opening failed\n");
    exit(1);
  }
  ret = write(mm_ctl, "start", 6);
  stats = fopen("/sys/devices/platform/mastermind/stats", "r");
  if (stats == NULL){
    printf("Opening failed\n");
    exit(1);
  }
  
  //test that if you send "start\n" to mm_ctl it will result in an error
  printf("Starting test 1: improper start with newline\n");
  errno = 0;
  ret = write(mm_ctl, "start\n", 6);
  CHECK_IS_NEGATIVE(ret);
  CHECK_IS_EQUAL(errno, EINVAL);
  //test that the program works with normal conditions
  //tests regualr start, an incorrect guess, correct guess, and quit
  printf("Starting test 2: happy path test with one incorrect guess\n");
  ret = write(mm_ctl, "start", 5);
  CHECK_IS_EQUAL(5, ret);
  ret = fprintf(mm, "0111");
  CHECK_IS_EQUAL(ret, 4);
  if (fgets(buf, 256, mm) <= 0){
    printf("Read failed\n");
  }
  prev = test_failed;
  CHECK_IS_EQUAL(strcmp("Guess 1: 2 black peg(s), 1 white peg(s)\n", buf), 0);
  if (test_failed > prev){
    printf("Looking for: Guess 1: 2 black peg(s), 2 white peg(s)\ninput was %s", buf);
  }
  fclose(mm);
  mm = fopen("/dev/mm", "r+");
  ret = fprintf(mm, "0012");
  CHECK_IS_EQUAL(ret, 4);
  if (fgets(buf, 256, mm) <= 0){
    printf("Read failed\n");
  }
  prev = test_failed;
  CHECK_IS_EQUAL(strcmp("Correct! Game Over.\n", buf), 0);
  if (test_failed > prev){
    printf("Correct! Game Over.\n");
  }
  ret = write(mm_ctl, "quit", 4);
  CHECK_IS_EQUAL(ret, 4);
  mm = fopen("/dev/mm", "r+");
  if (fgets(buf, 256, mm) <= 0){
    printf("Read failed\n");
  }
  CHECK_IS_EQUAL(strcmp("Game over. The code was 0012.\n", buf), 0);
  //tests that sending a guess with a number greater than 6 results in an error 
  printf("Starting test 3: numbers greater than 6\n");
  ret = write(mm_ctl, "start", 5);
  fclose(mm);
  mm = fopen("/dev/mm", "r+");
  mm2 = open("/dev/mm", O_RDWR);
  errno = 0;
  ret = write(mm2, "0082", 4);
  CHECK_IS_NEGATIVE(ret);
  CHECK_IS_EQUAL(errno, EINVAL);
   //test that sending more than 4 numbers, only the first 4 are taken
  printf("Starting test 4: sending more than 4 digits\n");
  fclose(mm);
  mm = fopen("/dev/mm", "r+");
  ret = fprintf(mm, "33331223344998");
  fclose(mm);
  mm = fopen("/dev/mm", "r+");
  CHECK_IS_EQUAL(ret, 14);
  if (fgets(buf, 256, mm) <= 0){
    printf("Read failed\n");
  }
  prev = test_failed;
  CHECK_IS_EQUAL(strcmp("Guess 1: 0 black peg(s), 0 white peg(s)\n", buf), 0);
  if (test_failed > prev){
    printf("Looking for: Guess 1: 0 black peg(s), 0 white peg(s)\ninput was %s", buf);
  }
  //tests that attempting to read from mm_ctl results in an error
  printf("Starting test 5: reading from mm_ctl\n");
  errno = 0;
  ret = read(mm_ctl, buf, 10);
  CHECK_IS_NEGATIVE(ret);
  CHECK_IS_EQUAL(errno, EINVAL);
  close(mm2);
  //tests the functionality of mmap
  ret = write(mm_ctl, "start", 5);
  CHECK_IS_EQUAL(5, ret);
  printf("Starting test 6: mmap\n");
  mm2 = open("/dev/mm", O_RDWR);
  ret = write(mm2, "0000", 4);
  CHECK_IS_EQUAL(ret, 4);
  close(mm2);
  mm2 = open("/dev/mm", O_RDWR);
  ret = write(mm2, "1234", 4);
  CHECK_IS_EQUAL(ret, 4);
  map = mmap(0, PAGE_SIZE, PROT_READ, MAP_PRIVATE, mm2, 0);
  if (map == MAP_FAILED){
    printf("Mapping failed\n");
  }
  CHECK_IS_EQUAL(strcmp("Guess 1: 0000  |  B2 W0\nGuess 2: 1234  |  B0 W2\n", (char*) map), 0);
  printf("%s\n", (char*) map);
  close(mm2);
  mm2 = open("/dev/mm", O_RDWR);
  ret = write(mm2, "1235", 4);
  CHECK_IS_EQUAL(ret, 4);
  CHECK_IS_EQUAL(strcmp("Guess 1: 0000  |  B2 W0\nGuess 2: 1234  |  B0 W2\nGuess 3: 1235  |  B0 W2\n", (char*) map), 0);
  printf("%s\n", (char*) map);
  close(mm_ctl);
  fclose(mm);
  close(mm2);
  
  //start of proj2 tests
  
  //need to test as root and as non root
  printf("Starting test 7: changing colors as non root\n");
  mm_ctl = open("/dev/mm_ctl", O_RDWR);
  mm2 = open("/dev/mm", O_RDWR);
  if (mm_ctl < 1 || mm2 < 1){
    printf("Opening failed\n");
    exit(1);
  }
  ret = write(mm_ctl, "start", 6);
  stats = fopen("/sys/devices/platform/mastermind/stats", "r");
  if (stats == NULL){
    printf("Opening failed\n");
    exit(1);
  }
  errno = 0;
  ret = write(mm_ctl, "colors 4", 9);
  //check that correct error was returned for non-root user
  CHECK_IS_NEGATIVE(ret);
  CHECK_IS_EQUAL(errno, EPERM);
  if (fread(buf, 1, 256, stats) <= 0){
    printf("Read failed\n");
  }
  //check that color was not changed if user is not root
  buf[166] = '\0';
  CHECK_IS_EQUAL(strcmp("CS421 Mastermind Stats\nNumber of pegs: 4\nNumber of colors: 6\nNumber of times code was changed: 0\nNumber of invalid code change attempts: 0\nNumber of games started: 1\n", buf), 0);
  close(mm_ctl);
  fclose(stats);
  close(mm2);
  stats = fopen("/sys/devices/platform/mastermind/stats", "r");
  mm_ctl = open("/dev/mm_ctl", O_RDWR);
  mm2 = open("/dev/mm", O_RDWR);
  printf("Starting test 9: proper interrupt\n");
  cs421net_init();
  ret = write(mm_ctl, "start", 6);
  if (!cs421net_send("3333", 4)){
    printf("Network send failed\n");
    exit(1);
  }
  sleep(1);
  ret = write(mm2, "3333", 4);
  if (ret <= 0){
    printf("write failed\n");
  }
  memset(buf, 0, 256);
  ret = read(mm2, buf, 256); 
  if (ret <= 0){
    printf("read failed\n");
  }
  //checks that the guess 3333 was correct
  CHECK_IS_EQUAL(strcmp("Correct! Game Over.\n", buf), 0);
  close(mm_ctl);
  fclose(stats);
  close(mm2);
  stats = fopen("/sys/devices/platform/mastermind/stats", "r");
  mm_ctl = open("/dev/mm_ctl", O_RDWR);
  mm2 = open("/dev/mm", O_RDWR);
  printf("Starting test 10: interrupt too long\n");
  ret = write(mm_ctl, "start", 6);
  if (!cs421net_send("33333", 5)){
    printf("Network send failed\n");
    exit(1);
  }
  sleep(1);
  ret = write(mm2, "0012", 4);
  if (ret <= 0){
    printf("write failed\n");
  }
  memset(buf, 0, 256);
  ret = read(mm2, buf, 256); 
  if (ret <= 0){
    printf("read failed\n");
  }
  //checks that the guess 0012 was correct, meaning code was not changed
  CHECK_IS_EQUAL(strcmp("Correct! Game Over.\n", buf), 0);

  close(mm_ctl);
  fclose(stats);
  close(mm2);
  stats = fopen("/sys/devices/platform/mastermind/stats", "r");
  mm_ctl = open("/dev/mm_ctl", O_RDWR);
  mm2 = open("/dev/mm", O_RDWR);
  printf("Starting test 11: interrupt not in bounds\n");
  ret = write(mm_ctl, "start", 6);
  if (!cs421net_send("3373", 3)){
    printf("Network send failed\n");
    exit(1);
  }
  sleep(1);
  ret = write(mm2, "0012", 4);
  if (ret <= 0){
    printf("write failed\n");
  }
  memset(buf, 0, 256);
  ret = read(mm2, buf, 256); 
  if (ret <= 0){
    printf("read failed\n");
  }
  //checks that the guess 0012 was correct, meaning code was not changed
  CHECK_IS_EQUAL(strcmp("Correct! Game Over.\n", buf), 0);
  close(mm_ctl);
  fclose(stats);
  close(mm2);
  printf("%u Tests passed, %u Tests failed\n", test_passed, test_failed);
  return 0;
}
