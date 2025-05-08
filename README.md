# Cursor-Control Brain-Computer Interface
My project develops a brain-computer interface (BCI) for controlling a computer cursor using motor imagery (imagined body movements). At this system's core, a machine learning model interprets neural signals from an electroencephalography (EEG) cap and, from that, infers how the cursor should move. 

I created a pipeline for recording and processing that data so that it can be used as input for the model. The outputs of the model are commands for how the cursor should move. 

## Directory Organization:
- The "Mischenko Dataset EEGNet" directory contains code for training the machine learning model and the dataset used for machine learning.
- The "Data_Collection" directory contains code for collecting data from an EEG cap, using machine learning to process that data, and move the cursor according to the model's predictions.