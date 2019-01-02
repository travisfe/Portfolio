import numpy as np
import csv

#gets predictions using trained model
def test(syn0, syn1, data):
        l0 = data
        l1 = nonlin(np.dot(l0,syn0))
        l2 = softmax(np.dot(l1,syn1))
        return l2


#Import and clean up data
def imp(path):
        f = csv.reader(open(path), delimiter = ",")
        temp = list(f)
        xy = np.array(temp)
        r = np.delete(np.delete(xy, 0, 0), 0, 1)
        return np.hsplit(r, [13, 16])

#converts y to binary classification
def conv(yt):
        y = []
        for x in yt:
                if x[2] <= 3:
                        y.append([1, 0])
                else:
                        y.append([0, 1])
        return y

#reformats data
def clean(x):
        arr = []
        for i in np.nditer(x, op_flags=['readwrite']):
                if (i == 'NA'):
                        i[...] = -1
                        arr.append(0)
                else:
                        arr.append(1)
        x = np.append(x, arr)
        return np.asarray(x)

#implimentation of softmax function and derivative
def softmax(lis, deriv=False):
        if deriv == True:
                return np.subtract(lis,lis ** 2)
        temp = np.exp(lis - np.max(lis, axis=-1, keepdims=True))
        temp2 = np.sum(temp, axis=-1, keepdims=True)
        end = temp / temp2
        return end


#sigmoid function and derivative
def nonlin(x,deriv=False):
        if(deriv==True):
                return x*(1-x)
        return 1/(1+np.exp(-x))

#initialize training data
end = np.empty((0,26), dtype='float64')
yend = []
#training set data is 7 files labeled "pathX_data.csv" where X is 1 through 7
#loop reformats data from each file and combines all into one large training set
for x in range(1, 7):
        arrs = imp("path%d_data.csv" % x)
        X = arrs[0]
        yt = arrs[1]
        yt = yt.astype('float64')
        y = conv(yt)
        for i in y:
                yend.append(i)
        X = np.apply_along_axis(clean, axis=1, arr=X)
        end = np.append(end, X, axis=0)


#convert types
X = end
y = yend
np.random.seed(4)
y = np.array(y)
X = X.astype('float64')
y = y.astype('float64')

#standardize data
X = ((X - X.mean()) / (X.std()))

# randomly initialize weights using he-et-al
syn0 = np.random.randn(26, 13) * np.sqrt(2/13)
syn1 = np.random.randn(13,2) * np.sqrt(2/2)

for j in xrange(60000):

        # Initialize the 3 layers of the neural net
        l0 = X
        l1 = nonlin(np.dot(l0,syn0))
        l2 = softmax(np.dot(l1,syn1))

        # Error with weight decay
        lam = .000001
        w = np.sum(np.abs(l2))
        l2_error = y - l2 + (lam * w)

        #print current error every 10000 iterations
        if (j% 10000) == 0:
                print "Error:" + str(np.mean(np.abs(l2_error)))

        # Determine how much to change weights for l2
        l2_delta = l2_error*softmax(l2,deriv=True)

        # l1 contribution to l2 error
        l1_error = l2_delta.dot(syn1.T)
        
        # determine how much to change wieghts for l1
        l1_delta = l1_error * nonlin(l1,deriv=True)

        syn1 += l1.T.dot(l2_delta)
        syn0 += l0.T.dot(l1_delta)


#initialize test data
tarrs = imp("test_path_data_soln.csv")
TX = tarrs[0]
tesy = tarrs[1]
tesy = tesy.astype('float64')
ty = conv(tesy)
ty = np.array(ty)
TX = np.apply_along_axis(clean, axis=1, arr=TX)

#change types
TX = TX.astype('float64')
ty = ty.astype('float64')

#standardize test data
TX = ((TX - TX.mean()) / (TX.std()))

#predict values for test data
guess = test(syn0, syn1, TX)

fin = []
comp = []
err = 0
close_err = 0
far_err = 0

#round predictions to 0 or 1
for x in range(0, len(guess)):
        fin.append(int(round(guess[x][0])))

#determine whether each prediction is correct or incorrect
for x in range(0, len(fin)):
        #create array of predition vs actual values
        comp.append([guess[x][0], guess[x][1], ty[x][0]])

        print "Prediction for Observation ", x + 1, " was: ",
        if (fin[x] == ty[x][0]):
                print "Correct!"
        else:
                #For missclassified data, determine whether it missclassified something close as being far away, or something far away as being close
                #Most things are far, and something being close to the sensors could mean a potential threat
                #we want to minimize missclassifying things that are actually close as much as possible
                print "Incorrect!"
                if ty[x][0] == 1:
                        close_err += 1
                else:
                        far_err += 1
                err += 1

#print results
print "Prediction vs Actual"
for x in range(0, len(comp)):
        print "Observation ", x + 1, ": ", comp[x]
print err, " Wrong out of ", len(ty), "With ", close_err, "closes missclassified and ", far_err, " non-closes missclassified classified"
