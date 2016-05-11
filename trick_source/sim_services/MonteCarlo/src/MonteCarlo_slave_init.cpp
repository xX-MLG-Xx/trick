
#include <sys/stat.h>
#include <libgen.h>

#include "sim_services/MonteCarlo/include/MonteCarlo.hh"
#include "sim_services/CommandLineArguments/include/command_line_protos.h"
#include "sim_services/Message/include/message_proto.h"
#include "sim_services/Message/include/message_type.h"
#include "trick_utils/comm/include/tc_proto.h"

/** @par Detailed Design: */
int Trick::MonteCarlo::slave_init() {
    /** <ul><li> Construct the run directory. */
    run_directory = std::string(command_line_args_get_output_dir());

    if (access(run_directory.c_str(), F_OK) != 0) {
        if (mkdir(run_directory.c_str(), 0775) == -1) {
            if (verbosity >= ERROR) {
                message_publish(MSG_ERROR, "Monte [%s:%d] : Unable to create directory %s.\nTerminating.\n",
                            run_directory.c_str(), machine_name.c_str(), slave_id) ;
            }
            exit(-1);
        }
    }

    /** <li> Run the slave initialization jobs. */
    run_queue(&slave_init_queue, "in slave_init queue") ;

    /** <li> Initialize the sockets. */
    tc_error(&listen_device, 0);
    tc_error(&connection_device, 0);
    tc_error(&data_listen_device, 0);
    tc_error(&data_connection_device, 0);
    socket_init(&listen_device);
    listen_device.disable_handshaking = TC_COMM_TRUE;

    /** <li> Connect to the master and write the port over which we are listening for new runs. */
    if (tc_connect(&connection_device) != TC_SUCCESS) {
        if (verbosity >= ERROR) {
            message_publish(MSG_ERROR, "Monte [%s:%d] : Failed to initialize communication sockets.\nTerminating.\n",
                            machine_name.c_str(), slave_id) ;
        }
        exit(-1);
    }

    if (verbosity >= ALL) {
        message_publish(MSG_INFO, "Monte [%s:%d] : Making initial connection with Master.\n",
                        machine_name.c_str(), slave_id) ;
    }

    int id = htonl(slave_id);
    tc_write(&connection_device, (char *)&id, (int)sizeof(id));

    char hostname[HOST_NAME_MAX];
    gethostname(hostname, sizeof(hostname)-1);

    int num_bytes = htonl(strlen(hostname));
    tc_write(&connection_device, (char *)&num_bytes, (int)sizeof(num_bytes));
    tc_write(&connection_device, hostname, strlen(hostname));
    int listen_port = htonl(listen_device.port);
    tc_write(&connection_device, (char *)&listen_port, (int)sizeof(listen_port));
    tc_disconnect(&connection_device);

    return 0;
}