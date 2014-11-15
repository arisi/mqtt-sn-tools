/*
  MQTT-SN command-line publishing client
  Copyright (C) 2013 Nicholas Humfrey

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "mqtt-sn.h"

const char *client_id = NULL;
const char *topic_name = NULL;
const char *message_data = NULL;
const char *t_topic_name = NULL;
const char *t_message_data = NULL;
time_t keep_alive = 1;
const char *mqtt_sn_host = "127.0.0.1";
const char *mqtt_sn_port = "1883";
uint16_t topic_id = 0;
uint8_t topic_id_type = MQTT_SN_TOPIC_TYPE_NORMAL;
int8_t qos = 0;
uint8_t retain = FALSE;
uint8_t debug = FALSE;


static void usage()
{
    fprintf(stderr, "Usage: mqtt-sn-pub [opts] -t <topic> -m <message>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -d             Enable debug messages.\n");
    fprintf(stderr, "  -h <host>      MQTT-SN host to connect to. Defaults to '%s'.\n", mqtt_sn_host);
    fprintf(stderr, "  -i <clientid>  ID to use for this client. Defaults to 'mqtt-sn-tools-' with process id.\n");
    fprintf(stderr, "  -m <message>   Message payload to send.\n");
    fprintf(stderr, "  -n             Send a null (zero length) message.\n");
    fprintf(stderr, "  -p <port>      Network port to connect to. Defaults to %s.\n", mqtt_sn_port);
    fprintf(stderr, "  -q <qos>       Quality of Service value (0 or -1). Defaults to %d.\n", qos);
    fprintf(stderr, "  -r             Message should be retained.\n");
    fprintf(stderr, "  -t <topic>     MQTT topic name to publish to.\n");
    fprintf(stderr, "  -T <topicid>   Pre-defined MQTT-SN topic ID to publish to.\n");
    fprintf(stderr, "  -w <topic>     MQTT LWT topic name to publish to.\n");
    fprintf(stderr, "  -W <message>   LWT Message payload to send.\n");
    exit(-1);
}

static void parse_opts(int argc, char** argv)
{
    int ch;

    // Parse the options/switches
    while ((ch = getopt(argc, argv, "dh:i:m:w:W:np:q:rt:T:?")) != -1)
        switch (ch) {
        case 'd':
            debug = TRUE;
        break;

        case 'h':
            mqtt_sn_host = optarg;
        break;

        case 'i':
            client_id = optarg;
        break;

        case 'm':
            message_data = optarg;
        break;

        case 'W':
            t_message_data = optarg;
        break;

        case 'n':
            message_data = "";
        break;

        case 'p':
            mqtt_sn_port = optarg;
        break;

        case 'q':
            qos = atoi(optarg);
        break;

        case 'r':
            retain = TRUE;
        break;

        case 't':
            topic_name = optarg;
        break;

        case 'w':
            t_topic_name = optarg;
        break;

        case 'T':
            topic_id = atoi(optarg);
        break;

        case '?':
        default:
            usage();
        break;
    }

    // Missing Parameter?
    if (!(topic_name || topic_id) || !message_data) {
        usage();
    }

    if (qos != -1 && qos != 0 && qos != 1) {
        fprintf(stderr, "Error: only QoS level 1, 0 or -1 is supported.\n");
        exit(-1);
    }

    // Both topic name and topic id?
    if (topic_name && topic_id) {
        fprintf(stderr, "Error: please provide either a topic id or a topic name, not both.\n");
        exit(-1);
    }

    // Check topic is valid for QoS level -1
    if (qos == -1 && topic_id == 0 && strlen(topic_name) != 2) {
        fprintf(stderr, "Error: either a pre-defined topic id or a short topic name must be given for QoS -1.\n");
        exit(-1);
    }
}

int main(int argc, char* argv[])
{
    int sock;

    // Parse the command-line options
    parse_opts(argc, argv);

    // Enable debugging?
    mqtt_sn_set_debug(debug);
    // Create a UDP socket
    sock = mqtt_sn_create_socket(mqtt_sn_host, mqtt_sn_port);
    if (sock) {
        // Connect to gateway
        if (qos >= 0) {
           if (qos==0) {
                mqtt_sn_send_connect(sock, client_id, MQTT_SN_FLAG_CLEAN, keep_alive);
                mqtt_sn_receive_connack(sock);
            } else if (qos==1) {
                mqtt_sn_send_connect(sock, client_id, MQTT_SN_FLAG_CLEAN | MQTT_SN_FLAG_WILL , keep_alive);
                connack_packet_t *p= mqtt_sn_receive_packet(sock);
                if ( p!= NULL && p->type==MQTT_SN_TYPE_WILLTOPICREQ) {
                    mqtt_sn_send_will_topic(sock, t_topic_name, MQTT_SN_FLAG_QOS_1 | MQTT_SN_FLAG_RETAIN);
                    p= mqtt_sn_receive_packet(sock);
                    if ( p!= NULL && p->type==MQTT_SN_TYPE_WILLMSGREQ) {
                        mqtt_sn_send_will_msg(sock, t_message_data);
                    } else {
                        fprintf(stderr,"Error: did not get MQTT_SN_TYPE_WILLMSGREQ.\n");
                        exit(-1);
                    }
                } else {
                    fprintf(stderr,"Error: did not get MQTT_SN_TYPE_WILLTOPICREQ.\n");
                    exit(-1);
                }
                mqtt_sn_receive_connack(sock);
            }
        }
        if (topic_id) {
            // Use pre-defined topic ID
            topic_id_type = MQTT_SN_TOPIC_TYPE_PREDEFINED;
        } else if (strlen(topic_name) == 2) {
            // Convert the 2 character topic name into a 2 byte topic id
            topic_id = (topic_name[0] << 8) + topic_name[1];
            topic_id_type = MQTT_SN_TOPIC_TYPE_SHORT;
        } else if (qos >= 0) {
             // Register the topic name
             mqtt_sn_send_register(sock, topic_name);
             topic_id = mqtt_sn_receive_regack(sock);
             topic_id_type = MQTT_SN_TOPIC_TYPE_NORMAL;
        }

        // Publish to the topic
        mqtt_sn_send_publish(sock, topic_id, topic_id_type, message_data, qos, retain);
        if (qos==1) {
            int retries=0;
            while (1) {
                connack_packet_t *p= mqtt_sn_receive_packet(sock);
                if ( p!= NULL && p->type==MQTT_SN_TYPE_PUBACK) {
                    break;
                } else 
                    fprintf(stderr,"Warn: QoS 1 and send not acked -- retrying.. (%d/10)\n",retries);
                if (retries++>10) {
                    fprintf(stderr,"Error: QoS 1 and send not acked, tried %d times\n",retries);
                    exit(-1);
                }
                sleep(1);
                mqtt_sn_send_publish(sock, topic_id, topic_id_type, message_data, qos, retain);
            }
            if (retries)
                fprintf(stderr,"Warn: Send required %d times, but was successful.\n",retries);

        }
        // Finally, disconnect
        if (qos >= 0) {
            mqtt_sn_send_disconnect(sock);
            connack_packet_t *p= mqtt_sn_receive_packet(sock);
            if ( p!= NULL && p->type==MQTT_SN_TYPE_DISCONNECT) {
                // disconnect acked ok
            } else 
                fprintf(stderr,"Warn: QoS >=0 and DISCONNECT not acked\n");
        }
        close(sock);
    }

    mqtt_sn_cleanup();

    return 0;
}
