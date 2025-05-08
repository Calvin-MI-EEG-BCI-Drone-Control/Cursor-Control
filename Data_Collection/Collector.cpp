#include "Collector.h"

#include <conio.h> //kbhit() for exiting the loop

// constants for file names
#define LOG_FILE "iworx.log"
#define CONFIG_FILE "../iWorxSettings/IX-EEG-Impedance-Check.iwxset"

// The window size used for the sliding window (in the jupyter notebook for machine learning)
#define WINDOW_SIZE 200 


// MQTT Constants
#define ADDRESS     "ssl://3d0ef2c001394874af7fdfa932b5e994.s1.eu.hivemq.cloud:8883"
#define CLIENTID    "EEG_Publisher"
#define SIZE_TOPIC	"EEG/size"
#define DATA_TOPIC	"EEG/data"
#define QOS         1
#define TIMEOUT     10000L

using namespace std;

// COMPILATON with iworx and sqlite and mqtt
// g++ Collector.cpp -o Collector -I"../iWorxDAQ_64" -L"../iWorxDAQ_64" -liwxDAQ -I"$VCPKG_ROOT/installed/x64-windows/include" -L"$VCPKG_ROOT/installed/x64-windows/lib" -lsqlite3 -lpaho-mqtt3cs

/** USAGE
 * ./Collector <database>
 */

 int main(int argc, char **argv)
 {
	 dataCollector(argc, argv);
	 return 0;
 }

/**
 * A callback for whenever an SQL query (sqlite3_exec()) returns values
 * @param argc: the number of values returned
 * @param argv: an array of the values returned
 * @param azColName: the name of the column each value in argv was returned from
 */
static int SQLcallback(void *NotUsed, int argc, char **argv, char **azColName){
    // int i;
    // for(i=0; i<argc; i++){
    //   printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    // }
    return 0;
}

static int handleSQLErrors(int returnCode, char *ErrorMessage) {
	if( returnCode!=SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", ErrorMessage);
      sqlite3_free(ErrorMessage);
    }
	return 0;
}

int startHardware(char* logfile) {
	// Open the iWorx Device. If it fails, return an error.
	if (!OpenIworxDevice(logfile)) {
		perror("\nERROR: Unable to open iWorx device");
		return 1;
	}

	//Find Hardware
	int model;
	char name_buffer[1000], sn_buffer[1000];
	FindHardware(&model, name_buffer, 1000, sn_buffer, 1000); // bufferSize  should be  atleast 16 bytes long
	printf("Hardware found: %s\nSerial Number: %s\n", name_buffer, sn_buffer);
	if (model < 0) {
		perror("\nERROR: No Hardware Found");
		return 1;
	}
	return 0;
}

/// MQTT FUNCTIONS START ///

int startMQTT(MQTTClient* client) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // Initialize the MQTT client
    if ((rc = MQTTClient_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        // ensure that you include the dll for paho-mqtt3cs for ssl and pahomqttc for tcp. If using the wrong one, it will fail to create the client and return error -14 (or similar error)
        printf("Failed to create client, return code %d\n", rc);
        return -1;
    };
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = "C_Pub";
    conn_opts.password = "C_Pub";

    // Will fail to connect and return error code -6 if these ssl options are not set.
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    ssl_opts.enableServerCertAuth = 0;
    // declare values for ssl options, here we use only the ones necessary for TLS, but you can optionally define a lot more
    // look here for an example: https://github.com/eclipse/paho.mqtt.c/blob/master/src/samples/paho_c_sub.c
    ssl_opts.verify = 1;
    ssl_opts.CApath = NULL;
    ssl_opts.keyStore = NULL;
    ssl_opts.trustStore = NULL;
    ssl_opts.privateKey = NULL;
    ssl_opts.privateKeyPassword = NULL;
    ssl_opts.enabledCipherSuites = NULL;

    // use TLS for a secure connection, "ssl_opts" includes TLS
    conn_opts.ssl = &ssl_opts;

    // Connect to the broker
    if ((rc = MQTTClient_connect(*client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        MQTTClient_destroy(client);
        // exit(EXIT_FAILURE);
        return -1;
    }
	return rc;
}

int endMQTT(MQTTClient* client) {
	// Disconnect from the broker
    MQTTClient_disconnect(*client, 10000);
    MQTTClient_destroy(client);
	return 0;
}


int publishData(MQTTClient client, MQTTClient_message pubmsg, float* sample, int sample_size) {
	int rc = MQTTCLIENT_SUCCESS;
	MQTTClient_deliveryToken token;
	pubmsg.payload = (void*)sample;
	pubmsg.payloadlen = sizeof(float) * sample_size;
	pubmsg.qos = QOS;
	pubmsg.retained = 0;
	
	rc = MQTTClient_publishMessage(client, DATA_TOPIC, &pubmsg, &token);
	if (rc != MQTTCLIENT_SUCCESS) {
		printf("Failed to publish message");
		printf("\nrc: %d", rc);
		MQTTClient_destroy(&client);
		exit(-1);
	}
	
	// DEBUGGING PRINT STATEMENT
	// printf("Publishing the message: "); 
	// for (int i = 0; i < 19; ++i) {
	// 	printf("%f, ", sample[i]);
	// }
	// printf("\n");

	rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
	if (rc != MQTTCLIENT_SUCCESS) {
		printf("Failed to complete message");
		MQTTClient_destroy(&client);
		exit(-1);
	}
	// printf("Message with delivery token %d delivered\n", token); // debugging
	return rc;
}

int publishSize(MQTTClient client, int size) {
	int rc = MQTTCLIENT_SUCCESS;
	MQTTClient_message sizemsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;
	sizemsg.payload = &size;
	sizemsg.payloadlen = sizeof(size);
	sizemsg.qos = QOS;
	sizemsg.retained = 0;
        
	rc = MQTTClient_publishMessage(client, SIZE_TOPIC, &sizemsg, &token);
	if (rc != MQTTCLIENT_SUCCESS) {
		printf("Failed to publish message");
		printf("\nrc: %d", rc);
		MQTTClient_destroy(&client);
		exit(-1);
	}

	// printf("Publishing message: %d\n", size); // debugging

	rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
	if (rc != MQTTCLIENT_SUCCESS) {
		printf("Failed to complete message");
		MQTTClient_destroy(&client);
		exit(-1);
	}

	// printf("Message with delivery token %d delivered\n", token); // debugging

	return rc;
}

/// MQTT FUNCTIONS END ///

/* collectData()
 * run a loop (the primary loop for this program) that reads data from the device and stores it in a SQLite database.
 * @param { MQTTClient } client
 * @param { MQTTClient_message } pubmsg
 * @param { sqlite3* } db: the name of the (existing) sqlite database
 * @param { int } num_channels_recorded: the number of channels to record from -- gotten from GetCurrentSamplingInfo()
 * @param { float } speed: the sampling speed -- gotten from GetCurrentSamplingInfo()
 */
int collectData(MQTTClient client, MQTTClient_message pubmsg, sqlite3 *db, int num_channels_recorded, float speed) {
	// VARIABLES AND CONSTANTS //
	// constants
	const int DATA_SIZE = 2000; // maximum datapoints to collect per call to ReadDataFromDevice()
	
	// variables for ReadDataFromDevice()
	int num_samples_per_ch = 0;
	long trig_index = -1;
	char trig_string[256];
	float data[DATA_SIZE];

	// used to time when data is read from the device.
	time_t record_time; 
	int read_num = 0;

	// the type of data currently being recorded
	string dataClass = "NA"; 

	int iRet; // return code
	unsigned total_datapoints; // total datapoints read by ReadDataFromDevice()

	// if using mqtt, publish the number of channels first.
	publishSize(client, num_channels_recorded);

	/// READ DATA ///
	/** 
	 * A function for calling ReadDataFromDevice() and related operations
	 * 
	 * @returns (by reference) 
	 * 
	 * - The datapoints gotten from ReadDataFromDevice (data)
	 * 
	 * - The time when it was recorded (record_time)
	 * 
	 * - The number of calls to ReadDataFromDevice that have been done so far (read_num)
	 */ 
	function readData = [&]() {
		iRet = ReadDataFromDevice(&num_samples_per_ch, &trig_index, trig_string, 256, data, DATA_SIZE);
		record_time = time(NULL);
		read_num++;
		if (num_samples_per_ch * num_channels_recorded > DATA_SIZE) printf("\nWARNING: amount of data recorded by ReadDataFromDevice() exceeds size of \"data\" buffer\n");
		// catch errors
		if (num_samples_per_ch < 0) {
			fprintf(stderr, "\nERROR: Invalid number of samples per channel");
			exit(1);
		}
	};

	/// STORE/SEND DATA ///
	/* A function used to both store the read data in a database
		and/or send it to the python program using MQTT */
	function processData = [&](bool store, bool mqtt) {
		string query = ""; // for dynamically creating a database query
		float sample_array[num_channels_recorded]; // an array for publishing data
		/// Add data to the database for each sample collected
		// for each sample gotten in the most recent read
		for (int j = 0; j < num_samples_per_ch; ++j) {
			// if our configuration is the impedance check
			if (CONFIG_FILE == "../iWorxSettings/IX-EEG-Impedance-Check.iwxset") {
				query = "INSERT INTO ImpMotorImagery VALUES(NULL,";
				// build a query for each sample, using data from each channel within the sample
				for (int k = 0; k < num_channels_recorded; ++k) {
					unsigned index = j * num_channels_recorded + k;
					if (index < DATA_SIZE) {
						if (store) query += to_string(data[index]) + ","; 
						if (mqtt) sample_array[k] = data[index];
					}
				}
				// add the window number, class, and time to the data
				if (store) {
					query += "\"" + dataClass + "\",\"" + asctime(gmtime(&record_time)) + "\");";
					// DEBUGGING: print out the built query
					// cout << query << endl;
				}
				if (mqtt) publishData(client, pubmsg, sample_array, num_channels_recorded);
			}

			if (store) {
				// add this datapoint to the SQLite database
				char *ErrMsg;
				int retCode = sqlite3_exec(db, query.c_str(), SQLcallback, 0, &ErrMsg);
				handleSQLErrors(retCode, ErrMsg);
			}
		}
	};

	/* Make sure we are not getting junk data.
		There is a delay between when the (iWorx) device is started and when meaningful data is recorded.
		If the array is full of 0s, wait for data. */
	do {
		readData();
		total_datapoints = num_channels_recorded * num_samples_per_ch;
	} while(std::all_of(data, data + total_datapoints, [](int x) { return x == 0; }));

	// main loop; while no buttons on the keyboard have been pressed
	cout << "Press any key to stop recording" << endl;
	while (!kbhit()) {
		readData();
			//do 	store, mqtt
		processData(true, true);
	}

	return 0;
}

/** startRecording()
 * Interact with the iworx API to obtain hardware details and start the hardware.
 * Also run collectData()
 */
int startRecording(sqlite3 *db) {
	// SETUP RECORDING

	startHardware(LOG_FILE);

	// Setup client and connect to MQTT broker
	MQTTClient client;
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	startMQTT(&client);

	int iRet = LoadConfiguration(CONFIG_FILE); // Load a settings file that has been created with LabScribe
	if (iRet != 0) {
		perror("\nERROR: Failure to load configuration");
		CloseIworxDevice();
		endMQTT(&client);
		return 1;
	}

	//Get current sampling speed and num of channels
	int num_channels_recorded;
	float speed;
	int sampInfo = GetCurrentSamplingInfo(&speed, &num_channels_recorded);

	// Start Acquisition
	iRet = StartAcq(speed * num_channels_recorded);
	if (iRet != 0) {
		perror("\nERROR: failed to start data acquisition");
		CloseIworxDevice();
		endMQTT(&client);
		return 1;
	}

	iRet = collectData(client, pubmsg, db, num_channels_recorded, speed);

	// Stop Acquisition
	StopAcq();
	printf("\nAquisition Stopped");

	// Close the iWorx Device
	CloseIworxDevice();
	// disconnect from broker and destroy client
	endMQTT(&client);
	return 0;
}

/** dataCollector()
  * set up the SQLite database and run startRecording()
  */
void dataCollector(int argc, char **argv) {
	// set up SQLite database
	sqlite3 *db;
	char *ErrMsg = 0;
	if (argc < 2) {
		perror("USAGE: ./DatasetCollector <database_name>\n");
		exit(1);
	}
	if( sqlite3_open(argv[1], &db) ){
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      sqlite3_close(db);
      exit(1);
    }
	
	// table to contain all values from the impedence check settings (IX-EEG-Impedance-Check.iwxset)
	/* NOTE: 
		The data from ReadDataFromDevice() has a shape of (19,).
	
		The last values (class and time):
		class: the class this datapoint belongs to
		time: the wall clock time when this data was recorded (when ReadDataFromDevice() was called) 
	*/
	char *impedenceTable = "";
	if (CONFIG_FILE == "../iWorxSettings/IX-EEG-Impedance-Check.iwxset") {
		impedenceTable = 
		"CREATE TABLE IF NOT EXISTS ImpMotorImagery ("
		"id INTEGER PRIMARY KEY, "
		"FP1 REAL, FP2 REAL, F7 REAL, F3 REAL, Fz REAL, F4 REAL, F8 REAL, "
		"T3 REAL, C3 REAL, Cz REAL, C4 REAL, T4 REAL, T5 REAL, "
		"P3 REAL, Pz REAL, P4 REAL, T6 REAL, O1 REAL, O2 REAL,"
		"class TEXT, time TEXT)";
	}
	// create a table if needed
	int retCode = sqlite3_exec(db, impedenceTable, SQLcallback, 0, &ErrMsg);
	handleSQLErrors(retCode, ErrMsg);

	startRecording(db);

	sqlite3_close(db);
}