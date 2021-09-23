# restDAQ
Generic Data AcQuisition Software for REST, which also provides a Graphical User Interface (GUI) to visualize and control the data acquisition.

The DAQ core software is under the `daq` folder, currently only DCC and dummy (random data generator) electronics are supported. The acquisition is launched via `restDAQManager` program, which can be controlled via shared memory. Standard operation mode is to launch `restDAQManager` without any argument, it will start the shared memory and wait for the acquitition to be started. Afterwards, the data acquisition can be launched using `REST_DAQGUI.C` macro (see below for more details). Some parameters, such as: configuration file, number of events or run type can be controlled via shared memory. Moreover, it is possible to launch the data acquisition via command line using `restDAQManager --c myDAQCfgFile.rml`. However, `restDAQManager` will exit once the data acquisition is stopped. Further options are provided to stop de on-going run `restDAQManager --s` or exit the DAQ Manager `restDAQManager --e`. Since the `restDAQManager` is using shared memory only one instance of `restDAQManager` is allowed. The data is stored in a root file using `TRestRawSignalEvent` event format. Moreover, some `TRestRawDAQMetadata` is stored to track the DAQ parameters used in a particular run.

The GUI core is under the `gui` folder, the GUI runs separatelly of the `restDAQManager` program. However, an instance of `restDAQManager` has to be running in order to manage the data acquisition. To launch the `gui` a macro is provided under `macros/REST_DAQGUI.C` which can be launched using `restRoot`. No arguments are required, but a decoding file has to be provided in order to display the event hitmap.

![image](https://user-images.githubusercontent.com/80903717/129692859-b64ae0ef-03ad-4609-89cc-ad28fcf27827.png)

The DAQ GUI provides an user interface in order to control the DAQ. Different input fields are provided such as `Run type` (background, calibration or pedestals), `CfgFile` (configuration file to be used) or `Nevents` (number of events to be acquired). Moreover, some output fields are displayed to monitor de run name, instantaneous rate, number of events acquired and acquisition status (STARTED, STOPPED or UNKNOWN). Finally, different buttons are implemented in order to control the acquisition: `START`, `STOP` and `QUIT`. Note that the `QUIT` button exit the GUI, however it doesn't stop the `restDAQManager` and thus the data acquisition can continue running even without a running GUI. The gui interfaces with `restDAQManager` via shared memory and some parameters can be retreived and set for the DAQ. Some run parameters are displayed in the GUI such as: rate (instantaneous and mean rate), spectrum (based on the max amplitude), pulses and hitmap (average X and Y position). The event display is performed in a separated thread inside the GUI code in which the output root file is readed and the different parameters are extracted.
