from DataStream import *
from tensorflow import keras
import numpy as np

WINDOW_SIZE = 200 # sliding window size. Should correspond with the window size used for the model.

def main():
    print("Preparing Data Stream...")
    stream = DataStream()

    # clear junk data (A delay between when the hardware is started and when meaningful data is being transmitted)
    temp = stream.get_data() 
    # if stream.get_data() returns none or an array of all 0s
    while temp == None or all(value == 0.0 for value in temp):
        # returns None if the function is called faster than the data is published/received
        temp = stream.get_data()

    print("Starting Data Collection")
    dataArray = np.empty((19, 0)) # storage array for the input data
    model = keras.models.load_model('EEG-Model.keras') # load model from file
    size = 0
    while True:
        newValue = stream.get_data() # returns an Array or None if no new data is available
        prediction = None
        if (newValue != None):
            # add a datapoint to our window (sliding window)
            dataArray = np.hstack([dataArray, np.array(newValue).reshape(19, 1)])
            size = size + 1
            # once we get WINDOW_SIZE + 1 samples...
            if (dataArray.shape[1] > WINDOW_SIZE):
                # remove the oldest sample (creating an array of size WINDOW_SIZE)
                dataArray = np.delete(dataArray, 0, axis=1)
                size = size - 1
                # print(dataArray.shape)
                # print(size)
                prediction = np.argmax(model.predict(dataArray.reshape(1, 19, 200, 1)))
        if (isinstance(prediction, np.ndarray) or prediction != None): 
            print(prediction)
        
main()