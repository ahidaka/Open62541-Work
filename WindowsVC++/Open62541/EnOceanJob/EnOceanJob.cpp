/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
* See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS //disable fopen deprication warning in msvs
#endif

#ifdef UA_NO_AMALGAMATION
# include <time.h>
# include "ua_types.h"
# include "ua_server.h"
# include "ua_config_standard.h"
# include "ua_network_tcp.h"
# include "ua_log_stdout.h"
#else
# include "open62541.h"
#endif

#include <signal.h>
#include <errno.h> // errno, EINTR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tchar.h>
#include <windows.h>

#undef UA_ENABLE_METHODCALLS


#define EO_TEMP_NAME "eo_temperature"
#define EO_HUMI_NAME "eo_humidity"
#define EO_CO2_NAME  "eo_co2"
#define EO_SW_NAME   "eo_switch"
#define EO_TEMP_ID   "eo.temperature"
#define EO_HUMI_ID   "eo.humidity"
#define EO_CO2_ID    "eo.co2"
#define EO_SW_ID     "eo.switch"

UA_Boolean running = true;
UA_Logger logger = UA_Log_Stdout;


static int base = 0;

static void
writeVariable(UA_Server *server, int data, char *name);

static int getTemperature();

static int getHumidity();

static int getCo2();

static int getSwitch();

static int getBridgeData(TCHAR *lpFileName)
{
	HANDLE hFile;
	//TCHAR *lpFileName = _T("C:\\temp\\test.txt");
	char szBuff[32];
	DWORD dwNumberOfReadBytes;
	DWORD dwNumberOfBytesWritten;
	BOOL bRet;

	hFile = CreateFile(lpFileName,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("CreateFile Failed\r\n");
		return 1;
	}

	bRet = ReadFile(hFile,
		szBuff,
		sizeof(szBuff) / sizeof(szBuff[0]),
		&dwNumberOfReadBytes,
		NULL);

	if (!bRet) {
		printf("ReadFile Failed\r\n");
		CloseHandle(hFile);
		return 1;
	}

	//WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
	//	szBuff,
	//	dwNumberOfReadBytes,
	//	&dwNumberOfBytesWritten,
	//	NULL);

	int data = ::atoi(szBuff);

	CloseHandle(hFile);

	//printf("<<%d>>\r\n", data);

	return data;
}

static int getTemperature()
{
	int data = getBridgeData(_T("C:\\temp\\temp.txt"));
	//printf("*temp=%d\r\n", data);
	return data;
}

static int getHumidity()
{
	int data = getBridgeData(_T("C:\\temp\\humi.txt"));
	//printf("*humi=%d\r\n", data);
	return data;
}

static int getCo2()
{
	int data = getBridgeData(_T("C:\\temp\\co2.txt"));
	//printf("*co2=%d\r\n", data);
	return data;
}

static int getSwitch()
{
	int data = getBridgeData(_T("C:\\temp\\switch.txt"));
	//printf("*switch=%d\r\n", data);
	return data;
}

static void
enoceanCallback(UA_Server *server, void *data) {
	int temp = getTemperature();
	int humi = getHumidity();
	int co2 = getCo2();
	int sw = getSwitch();

	const int textSize = 128;
	char text[textSize];

	sprintf(text, "enoceancallback: %d %d %d %d", temp, humi, co2, sw);

	UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, (char *)text);

	writeVariable(server, temp, EO_TEMP_ID);
	writeVariable(server, humi, EO_HUMI_ID);
	writeVariable(server, co2, EO_CO2_ID);
	writeVariable(server, sw, EO_SW_ID);
}

static void stopHandler(int sign) {
	UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "Received Ctrl-C");
	running = 0;
}

/* Datasource Example */
static UA_StatusCode
readTimeData(void *handle, const UA_NodeId nodeId, UA_Boolean sourceTimeStamp,
	const UA_NumericRange *range, UA_DataValue *value) {
	if (range) {
		value->hasStatus = true;
		value->status = UA_STATUSCODE_BADINDEXRANGEINVALID;
		return UA_STATUSCODE_GOOD;
	}
	UA_DateTime currentTime = UA_DateTime_now();
	UA_Variant_setScalarCopy(&value->value, &currentTime, &UA_TYPES[UA_TYPES_DATETIME]);
	value->hasValue = true;
	if (sourceTimeStamp) {
		value->hasSourceTimestamp = true;
		value->sourceTimestamp = currentTime;
	}
	return UA_STATUSCODE_GOOD;
}


/**
* Now we change the value with the write service. This uses the same service
* implementation that can also be reached over the network by an OPC UA client.
*/

static void
writeVariable(UA_Server *server, int data, char *idName) {
	UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, idName);

	/* Write a different integer value */
	UA_Int32 myInteger = data; //43
	UA_Variant myVar;
	UA_Variant_init(&myVar);
	UA_Variant_setScalar(&myVar, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
	UA_Server_writeValue(server, myIntegerNodeId, myVar);

	/* Set the status code of the value to an error code. The function
	* UA_Server_write provides access to the raw service. The above
	* UA_Server_writeValue is syntactic sugar for writing a specific node
	* attribute with the write service. */
	UA_WriteValue wv;
	UA_WriteValue_init(&wv);
	wv.nodeId = myIntegerNodeId;
	wv.attributeId = UA_ATTRIBUTEID_VALUE;
	wv.value.status = UA_STATUSCODE_BADNOTCONNECTED;
	wv.value.hasStatus = true;
	UA_Server_write(server, &wv);

	/* Reset the variable to a good statuscode with a value */
	wv.value.hasStatus = false;
	wv.value.value = myVar;
	wv.value.hasValue = true;
	UA_Server_write(server, &wv);
}

static void
addVariable(UA_Server *server, char *name, char *id, UA_Int32 initValue) {
	/* add a static variable node to the server */
	UA_VariableAttributes myVar;
	UA_VariableAttributes_init(&myVar);

	UA_Int32 myInteger = initValue;

	myVar.description = UA_LOCALIZEDTEXT("en_US", name);
	myVar.displayName = UA_LOCALIZEDTEXT("en_US", name);
	myVar.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

	UA_Variant_setScalarCopy(&myVar.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
	const UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, name);
	const UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, id);

	UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
	UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
	UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId, parentReferenceNodeId,
		myIntegerName, UA_NODEID_NULL, myVar, NULL, NULL);
	UA_Variant_deleteMembers(&myVar.value);
}

int main(int argc, char** argv) {
	signal(SIGINT, stopHandler); /* catches ctrl-c */

	UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, 16664);
	UA_ServerConfig config = UA_ServerConfig_standard;
	config.networkLayers = &nl;
	config.networkLayersSize = 1;

	UA_Server *server = UA_Server_new(config);

	addVariable(server, EO_TEMP_NAME, EO_TEMP_ID, 0);
	addVariable(server, EO_HUMI_NAME, EO_HUMI_ID, 0);
	addVariable(server, EO_CO2_NAME, EO_CO2_ID, 0);
	addVariable(server, EO_SW_NAME, EO_SW_ID, 0);

	/* add a variable with the datetime data source */
	//UA_DataSource dateDataSource = (UA_DataSource) { .handle = NULL, .read = readTimeData, .write = NULL };
	UA_DataSource dateDataSource;
	dateDataSource.handle = NULL, dateDataSource.read = readTimeData, dateDataSource.write = NULL;

	UA_VariableAttributes v_attr;
	UA_VariableAttributes_init(&v_attr);
	v_attr.description = UA_LOCALIZEDTEXT("en_US", "current time");
	v_attr.displayName = UA_LOCALIZEDTEXT("en_US", "current time");
	v_attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
	const UA_QualifiedName dateName = UA_QUALIFIEDNAME(1, "current time");
	UA_NodeId dataSourceId;
	UA_Server_addDataSourceVariableNode(server, UA_NODEID_NULL, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), dateName,
		UA_NODEID_NULL, v_attr, dateDataSource, &dataSourceId);



	/* Add folders for demo information model */
#define DEMOID 50000
#define SCALARID 50001
	//#define ARRAYID 50002
	//#define MATRIXID 50003
	//#define DEPTHID 50004

	UA_ObjectAttributes object_attr;
	UA_ObjectAttributes_init(&object_attr);
	object_attr.description = UA_LOCALIZEDTEXT("en_US", "Demo");
	object_attr.displayName = UA_LOCALIZEDTEXT("en_US", "Demo");
	UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, DEMOID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "Demo"),
		UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), object_attr, NULL, NULL);

	object_attr.description = UA_LOCALIZEDTEXT("en_US", "Scalar");
	object_attr.displayName = UA_LOCALIZEDTEXT("en_US", "Scalar");
	UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, SCALARID),
		UA_NODEID_NUMERIC(1, DEMOID), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
		UA_QUALIFIEDNAME(1, "Scalar"),
		UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), object_attr, NULL, NULL);

	/* Fill demo nodes for each type*/
	UA_UInt32 id = 51000; // running id in namespace 0
	for (UA_UInt32 type = 0; type < UA_TYPES_DIAGNOSTICINFO; type++) {
		if (type == UA_TYPES_VARIANT || type == UA_TYPES_DIAGNOSTICINFO)
			continue;

		UA_VariableAttributes attr;
		UA_VariableAttributes_init(&attr);
		attr.valueRank = -2;
		attr.dataType = UA_TYPES[type].typeId;
#ifndef UA_ENABLE_TYPENAMES
		char name[15];
#if defined(_WIN32) && !defined(__MINGW32__)
		sprintf_s(name, 15, "%02d", type);
#else
		sprintf(name, "%02d", type);
#endif
		attr.displayName = UA_LOCALIZEDTEXT("en_US", name);
		UA_QualifiedName qualifiedName = UA_QUALIFIEDNAME(1, name);
#else
		attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US", UA_TYPES[type].typeName);
		UA_QualifiedName qualifiedName = UA_QUALIFIEDNAME_ALLOC(1, UA_TYPES[type].typeName);
#endif
		attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
		attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
		attr.userWriteMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;

		/* add a scalar node for every built-in type */
		void *value = UA_new(&UA_TYPES[type]);
		UA_Variant_setScalar(&attr.value, value, &UA_TYPES[type]);
		UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1, ++id),
			UA_NODEID_NUMERIC(1, SCALARID), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
			qualifiedName, UA_NODEID_NULL, attr, NULL, NULL);
		UA_Variant_deleteMembers(&attr.value);
	}

	/* Add the variable to some more places to get a node with three inverse references for the CTT */
	UA_ExpandedNodeId temp_nodeid = UA_EXPANDEDNODEID_STRING(1, EO_TEMP_ID);
	UA_ExpandedNodeId humi_nodeid = UA_EXPANDEDNODEID_STRING(1, EO_HUMI_ID);
	UA_ExpandedNodeId co2_nodeid = UA_EXPANDEDNODEID_STRING(1, EO_CO2_ID);
	UA_ExpandedNodeId sw_nodeid = UA_EXPANDEDNODEID_STRING(1, EO_SW_ID);

	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, DEMOID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), temp_nodeid, true);
	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, SCALARID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), temp_nodeid, true);

	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, DEMOID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), humi_nodeid, true);
	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, SCALARID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), humi_nodeid, true);

	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, DEMOID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), co2_nodeid, true);
	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, SCALARID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), co2_nodeid, true);

	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, DEMOID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), sw_nodeid, true);
	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, SCALARID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), sw_nodeid, true);

	/* Example for manually setting an attribute within the server */
	UA_LocalizedText objectsName = UA_LOCALIZEDTEXT("en_US", "Objects");
	UA_Server_writeDisplayName(server, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), objectsName);

	UA_Job job; job.type = UA_Job::UA_JOBTYPE_METHODCALL,
		job.job.methodCall.method = enoceanCallback, job.job.methodCall.data = NULL;

	//UA_Server_addRepeatedJob(server, job, 2000, NULL); /* call every 2 sec */
	UA_Server_addRepeatedJob(server, job, 10 * 1000, NULL); /* call every 10 sec */

															/* run server */
	UA_StatusCode retval = UA_Server_run(server, &running); /* run until ctrl-c is received */

															/* deallocate certificate's memory */
															//UA_ByteString_deleteMembers(&config.serverCertificate);
	UA_Server_delete(server);
	nl.deleteMembers(&nl);
	return (int)retval;
}
