# Mischenko Dataset EEGNet

- This directory contains code used to train the ML model. It is based on the [EEGNet architecture]( https://github.com/vlawhern/arl-eegmodels) and [an online EEG dataset](https://figshare.com/collections/_/3917698) (in which "Mischenko" refers to one of the contributors).

## Files
- The notebook (Mischenko-EEGNet.ipynb) contains code used to filter the data and train the model.
- EEG-Model.keras contains the weights from the trained model. This is used in Predictions.py.
- EEGModels.py contains the code for EEGNet.
- Other files include csv-formatted data from the online dataset.