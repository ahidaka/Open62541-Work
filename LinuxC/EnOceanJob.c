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
#include <unistd.h>

#define EXTERNAL_BROKER 1
#include "../dpride/typedefs.h"
#include "../dpride/utils.h"
#include "../dpride/dpride.h"
#include "../dpride/node.c"
#include "../dpride/eologfile.c"
#include "../dpride/EoControl.c"

static const char version[] = "\n@ open62541-dd Version 1.10 \n";

#undef UA_ENABLE_METHODCALLS

#define EO_DIRECTORY "/var/tmp/dpride"
#define UA_PID_FILE "opcua.pid"

#define DEFAULT_PORT (16664)
#define DOMAIN_LENGTH (256)

#define SIGENOCEAN (SIGRTMIN + 6)

#if 0
#define SC_SIZE (16)
#define NODE_TABLE_SIZE (256)
#define EO_DATSIZ (8)
typedef struct _eodata {
        int  Index;
        int  Id;
        char *Eep;
        char *Name;
        char *Desc;
        int  PIndex;
        int  PCount;
        char Data[EO_DATSIZ];
}
EO_DATA;
#endif

void EoSignalAction(int signo, void (*func)(int));
void ExamineEvent(int Signum, siginfo_t *ps, void *pt);
char *EoMakePath(char *Dir, char *File);
INT EoReflesh(void);
EO_DATA *EoGetDataByIndex(int Index);
FILE *EoLogInit(char *Prefix, char *Extension);
void EoLog(char *id, char *eep, char *msg);

//
int Port;
char *Domain;
int Dflag = 0;
int Lflag = 0;

typedef FILE* HANDLE;
typedef char TCHAR;
typedef int BOOL;

UA_Boolean running = true;
UA_Logger logger = UA_Log_Stdout;

enum EventStatus {
	NoEntry = 0,
	NoData = 1,
	DataExists = 2
};

enum EventStatus PatrolTable[NODE_TABLE_SIZE];

//
static void
writeVariable(UA_Server *server, int data, char *name);

static void
enoceanCallback(UA_Server *server, void *data)
{
	enum {itemLength = 64,
	      fullNameLength = 128,
	      bufferLength = BUFSIZ/4,
	};
	int i;
        EO_DATA *pe;
	enum EventStatus es;
	double value;
	char text[bufferLength];
	char logBuffer[bufferLength];
	char item[itemLength];
	char fullName[fullNameLength];
	int remainLength = fullNameLength - strlen(Domain) - 1;

	for (i = 0; i < NODE_TABLE_SIZE; i++) {
		es = PatrolTable[i];
		if (es == NoEntry) {
			break;
		}
		else if (es == DataExists) {
			PatrolTable[i] = NoData;
			strcpy(text, "enoceancallback: ");
			while((pe = EoGetDataByIndex(i)) != NULL) {
				value = strtod(pe->Data, NULL);
				if (Dflag | Lflag) {
					sprintf(logBuffer, "%d: %08X %d:%s=(%s)",
						i, pe->Id, pe->PIndex, pe->Name, pe->Data);
					if (Dflag) {
						printf("Debug:%s\n", logBuffer);
					}
					if (Lflag) {
						EoLog(Domain, logBuffer, "");
					}
				}
				strcpy(fullName, Domain);
				strncat(fullName, pe->Name, remainLength);
				writeVariable(server, value, fullName);
				sprintf(item, "%s=%s ",  fullName, pe->Data);
				strncat(text, item, (bufferLength - 1) - strlen(text));
			}
			UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, (char *)text);
		}
	}
}

//
//
static char *PidPath;

//
//
//
void EoSignalAction(int signo, void (*func)(int))
{
        struct sigaction act, oact;

        if (signo == SIGENOCEAN)
        {
                act.sa_sigaction = (void(*)(int, siginfo_t *, void *)) func;
                sigemptyset(&act.sa_mask);
                act.sa_flags = SA_SIGINFO;
        }
        else
        {
                act.sa_handler = func;
                sigemptyset(&act.sa_mask);
                act.sa_flags = SA_RESTART;
        }
        if (sigaction(signo, &act, &oact) < 0)
        {
                fprintf(stderr, "error at sigaction\n");
        }
}

void ExamineEvent(int Signum, siginfo_t *ps, void *pt)
{
	int index;
        char message[6] = {">>>0\n"};
        index = (unsigned long) ps->si_value.sival_int;
	PatrolTable[index] = DataExists;
        message[3] = '0' + index;
        write(1, message, 6);
}

//
//
//
static void stopHandler(int sign)
{
	UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "Received Ctrl-C");

	if (PidPath) {
                unlink(PidPath);
        }
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

int
main(int argc, char *argv[])
{
	int i;
	int opt;
	int itemCount;
	int totalCount;
	pid_t myPid = getpid();
	FILE *f;
	Port = DEFAULT_PORT;
        EO_DATA *pe;
	enum {
		fullNameLength = 128
        };
	char fullName[fullNameLength];
	int remainLength;

	printf(version);
	
	Domain = "\0"; /* Defailt Domain name is NULL */ 
	while ((opt = getopt(argc, argv, "Dd:Lp:")) != EOF) {
		switch (opt) {
		case 'D':
			Dflag++;
			break;

		case 'L':
			Lflag++;
			break;

		case 'd':
			Domain = strndup(optarg, DOMAIN_LENGTH);
			break;

		case 'p':
			Port = atoi(optarg);
			break;

		default: /* '?' */
			fprintf(stderr, "Usage: %s [-D][-d domain-name] [-p] port#\n",
				argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	remainLength = fullNameLength - strlen(Domain) - 1;

        PidPath = EoMakePath(EO_DIRECTORY, UA_PID_FILE);
        f = fopen(PidPath, "w");
        if (f == NULL)
        {
                fprintf(stderr, ": cannot create pid file=%s\n",
                        PidPath);
                return 1;
        }
        fprintf(f, "%d\n", myPid);
        fclose(f);
        printf("PID=%d file=%s\n", myPid, PidPath);
	printf("%s: D=%d; L=%d; domain=%s; port=%d; optind=%d\n",
	       argv[0], Dflag, Lflag, Domain, Port, optind);

	if (Lflag) {
		char buf[12];
		sprintf(buf, "%d", Port);
		(void) EoLogInit("opc", ".log");
                EoLog("Start", Domain, buf);
	}
	
	signal(SIGINT, stopHandler); /* catches ctrl-c */
	signal(SIGTERM, stopHandler); /* catches kill -15 */
        EoSignalAction(SIGENOCEAN, (void(*)(int)) ExamineEvent);

	UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, Port);
	UA_ServerConfig config = UA_ServerConfig_standard;
	config.networkLayers = &nl;
	config.networkLayersSize = 1;

	UA_Server *server = UA_Server_new(config);

	EoReflesh();
	if (Lflag) {
		EoLog("EoReflesh", "", "");
	}
	totalCount = 0;
	for(i = 0; i < NODE_TABLE_SIZE; i++) {
		itemCount = 0;
		totalCount++;
		while((pe = EoGetDataByIndex(i)) != NULL)
                {
			itemCount++;
			strcpy(fullName, Domain);
			strncat(fullName, pe->Name, remainLength);
			addVariable(server, pe->Name, fullName, 0);
		}
		if (itemCount == 0) {
			printf("Found last line=%d\n", i);
			break;
		}
		PatrolTable[i] = NoData;
	}
	
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
#if 0
	UA_ExpandedNodeId temp_nodeid = UA_EXPANDEDNODEID_STRING(1, EO_TEMP_ID);
	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, DEMOID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), temp_nodeid, true);
	UA_Server_addReference(server, UA_NODEID_NUMERIC(1, SCALARID),
		UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), temp_nodeid, true);
#endif
	for(i = 0; i < totalCount; i++) {
		UA_ExpandedNodeId extended_nodeid;
		while((pe = EoGetDataByIndex(i)) != NULL)
                {
			strcpy(fullName, Domain);
			strncat(fullName, pe->Name, remainLength);
			extended_nodeid = UA_EXPANDEDNODEID_STRING(1, fullName);
			UA_Server_addReference(server, UA_NODEID_NUMERIC(1, DEMOID),
					       UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), extended_nodeid, true);
			UA_Server_addReference(server, UA_NODEID_NUMERIC(1, SCALARID),
					       UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), extended_nodeid, true);
		}
	}
	
	/* Example for manually setting an attribute within the server */
	UA_LocalizedText objectsName = UA_LOCALIZEDTEXT("en_US", "Objects");
	UA_Server_writeDisplayName(server, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), objectsName);

	//UA_Job job; job.type = UA_Job::UA_JOBTYPE_METHODCALL,
	UA_Job job; job.type = UA_JOBTYPE_METHODCALL,
		job.job.methodCall.method = enoceanCallback, job.job.methodCall.data = NULL;

	//UA_Server_addRepeatedJob(server, job, 2000, NULL); /* call every 2 sec */
	UA_Server_addRepeatedJob(server, job, 1 * 1000, NULL); /* call every 1 sec */
															/* run server */
	UA_StatusCode retval = UA_Server_run(server, &running); /* run until ctrl-c is received */

															/* deallocate certificate's memory */
															//UA_ByteString_deleteMembers(&config.serverCertificate);
	UA_Server_delete(server);
	nl.deleteMembers(&nl);
	return (int)retval;
}
