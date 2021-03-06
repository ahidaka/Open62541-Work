/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
* See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */
//extern "C" {
#include <signal.h>

#ifdef UA_NO_AMALGAMATION
# include "ua_types.h"
# include "ua_server.h"
# include "ua_config_standard.h"
# include "ua_network_tcp.h"
# include "ua_log_stdout.h"
#else
# include "open62541.h"
#endif

static void
	testCallback(UA_Server *server, void *data) {
	UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "testcallback");
}

UA_Boolean running = true;
static void stopHandler(int sign) {
	UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
	running = false;
}

int main(void) {
	signal(SIGINT, stopHandler);
	signal(SIGTERM, stopHandler);

	UA_ServerConfig config = UA_ServerConfig_standard;
	UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, 16664);
	config.networkLayers = &nl;
	config.networkLayersSize = 1;
	UA_Server *server = UA_Server_new(config);

	/* add a repeated job to the server */
	//UA_Job job = { .type = UA_JOBTYPE_METHODCALL,
	//	.job.methodCall = { .method = testCallback,.data = NULL } };
	//enum { UA_JOBTYPE_METHODCALL = 4 };
	//UA_Job job; job.type = UA_JOBTYPE_METHODCALL,
	UA_Job job; job.type = UA_Job::UA_JOBTYPE_METHODCALL,
		job.job.methodCall.method = testCallback, job.job.methodCall.data = NULL;
	UA_Server_addRepeatedJob(server, job, 2000, NULL); /* call every 2 sec */

	UA_Server_run(server, &running);
	UA_Server_delete(server);
	nl.deleteMembers(&nl);
	return 0;
}
//}
