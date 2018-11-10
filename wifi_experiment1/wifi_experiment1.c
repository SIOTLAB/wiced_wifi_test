// Client to send information with RSSI, Noise, Transfer Rate, and Time to a server using a custom protocol (WWEP).
// The message sent and the response from the server are echoed to a UART terminal.
#include "wiced.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
//#include <utilities/cJSON/cJSON.h> //!Special include?
#include <cJSON.c>
#include <stdint.h>

//#define verbose

#define TCP_CLIENT_STACK_SIZE       (6200)
#define SERVER_PORT                 (14900)
#define PACKETSENDTHREADPRIORITY    (0)

//default values
#define MAX_EXP_NUM                     (10)
#define BURST                       (3)         //TEST
#define PACKETS_PER_BURST           (3)         //TEST
#define BURST_DELAY                 (0)         //the delay between bursts
#define EXP_DELAY                   (0)         //the delay between experiments
#define TX_POWER                    (0)        //transmission power

/* TODO: Delete */
//constants
#define TCP                         (0)
#define UDP                         (1)
#define BROADCAST                   (1)
#define UNICAST                     (0)


/* Misc. Variables */
static wiced_ip_address_t serverAddress;     // address of the WWEP server
static wiced_thread_t packetSendThread;      // Thread that sends packets
wiced_tcp_socket_t tcp_socket;               // The TCP socket
wiced_udp_socket_t udp_socket;               // The UDP socket

// static configuration
//int static_ip_address[4]    = {192, 168, 3, 20};
//int static_ap_address[4] = {192, 168, 3, 1};
//int static_netmask[4] = {255, 255, 255, 0};

// dhcp ip configuration
int broadcast_address[4]    = {192, 168, 3, 255};   // broadcast address
int server_address[4] = {192, 168, 1, 255};   // raspberry pi address
char ssid_str[128];
char pass_str[128];
uint32_t buf_size = 4096; //Check message
char buf[4096];


wiced_bool_t use_dhcp = WICED_FALSE;


/* Global variables to collect */
volatile int32_t rssi_val;          //XY.Z
volatile uint32_t rate_val;         //X144.Y
volatile int32_t noise_val;         //XY.Z
wiced_time_t time_val = 0;          //time in milliseconds


#define RX_BUFFER_SIZE    64
wiced_ring_buffer_t rx_buffer;
uint8_t rx_data[RX_BUFFER_SIZE];
wiced_uart_config_t uart_config =
{
    .baud_rate    = 115200,
    .data_width   = DATA_WIDTH_8BIT,
    .parity       = NO_PARITY,
    .stop_bits    = STOP_BITS_1,
    .flow_control = FLOW_CONTROL_DISABLED,
};


/* Variables for the user to input */
struct init_params {
    char* ip;                             //the IP address
    char* subnet_mask;                    //the subnet mask
    char*  gateway;                       //the gateway
    char* ssid;                           //the SSID
    char* pass;                           //the password
    uint32_t num_exp;                     //the number of experiments

};

/* Variables for the user to input */
struct exp_params {
    char* target_ip;                                       //Target Ip to send packets in experiment to
    uint32_t num_burst;                             //the number of packet bursts per experiment
    uint32_t packets_per_burst;                     //the number of packets sent per burst
    uint8_t  ack_enable_disable;                    //flag to disable/enable acknowledgments (aka turn on or off broadcast packets)
    uint32_t burst_delay;                           //the delay between bursts
    uint32_t exp_delay;                             //the delay between experiments
    uint32_t tcp_udp;                               //0 to use UDP or 1 to use TCP
    uint16_t junkMessageSize;                       //the size of the junk message
    uint16_t powerSaveMode;
    uint8_t type_of_service;                        //type of service, where 0x00 or 0xC0 = Best effort, 0x40 or 0x80 = Background, 0x20 or 0xA0 = Video, 0x60 or 0xE0 = Voice
                                                    //depending on the TOS, not all of the packets will transmit
    uint8_t li;                                     //listen interval--0 is default
    //uint16_t scan_interval;                       //wifi scan interval
};

//const char *jsonString[2] = {
//     "{"
//                "\"burst_num\": 1,"
//                "\"packet_num\": 2,"
//                "\"transport_protocol\": \"udp\","
//                "\"tos\": \"Background\","
//                "\"ack\": \"false\","
//                "\"delay_burst\": 3,"
//                "\"delay_exp\": 4,"
//                "\"size_msg\": 5,"
//                "\"psm\": \"false\""
//      "}",
//      "{"
//                  "\"burst_num\": 2,"
//                  "\"packet_num\": 3,"
//                  "\"transport_protocol\": \"tcp\","
//                  "\"tos\": \"Voice\","
//                  "\"ack\": \"true\","
//                  "\"delay_burst\": 4,"
//                  "\"delay_exp\": 5,"
//                  "\"size_msg\": 6,"
//                  "\"psm\": \"true\""
//       "}"
//};
//
//const char *jsonStringInit =
//"{"
//        "\"ip\": \"192.168.1.190\","
//        "\"subnet_mask\": \"255.255.255.255\","
//        "\"gateway\": \"192.168.1.1\","
//        "\"ssid\": \"CIA_AP\","
//        "\"pass\": \"glen_rox\","
//        "\"num_exp\": 5"
//"}";

struct exp_params test[MAX_EXP_NUM];
struct init_params login_struct;


//#define INIT_EXP_CASE_FUNC(exp_no, num_burst, packets_per_burst, ack_enable_disable, burst_delay, exp_delay, tcp_udp, junkMessageSize, powerSaveMode, type_of_service, li)    \
//    ({(exp_no).num_burst= num_burst, (exp_no).packets_per_burst=(packets_per_burst), (exp_no).ack_enable_disable=(ack_enable_disable), (exp_no).burst_delay=(burst_delay), \
//    (exp_no).exp_delay=(exp_delay), (exp_no).tcp_udp=(tcp_udp), (exp_no).junkMessageSize=(junkMessageSize), (exp_no).powerSaveMode=(powerSaveMode), (exp_no).type_of_service=(type_of_service), (exp_no).li=(li);})


////Function initializes the experiment test cases
//void init_struct()
//{
//    uint32_t num_burst          = BURST;
//    uint32_t packets_per_burst  = PACKETS_PER_BURST;
//    uint8_t  ack_enable_disable = 0;
//    uint32_t burst_delay        = BURST_DELAY;
//    uint32_t exp_delay          = EXP_DELAY;
//    uint32_t tcp_udp            = 0;
//    uint16_t junkMessageSize    = 100;
//    uint16_t powerSaveMode      = 0;  // 0:None, 1: PSM
//    // The type of service, where 0x00 or 0xC0 = Best effort, 0x40 or 0x80 = Background, 0x20 or 0xA0 = Video, 0x60 or 0xE0 = Voice
//    uint8_t type_of_service     = 0x00;
//    uint8_t li                  = 10;
//
//    INIT_EXP_CASE_FUNC(test[0], num_burst, packets_per_burst, ack_enable_disable, burst_delay, exp_delay, tcp_udp, junkMessageSize, powerSaveMode, type_of_service, li);
//    //INIT_EXP_CASE_FUNC(test[1], num_burst, packets_per_burst, ack_enable_disable, burst_delay, exp_delay, tcp_udp, junkMessageSize, type_of_service, li);
//
//
//#ifdef verbose
//    //set tx power at the beginning of each experiment
//    if(wwd_wifi_set_tx_power( TX_POWER ) != WWD_SUCCESS)
//        WPRINT_APP_INFO(("%s\n","Power could not set"));
////    else
////        WPRINT_APP_INFO(("Power set to: %d\n", TX_POWER));
//#endif
//
//}

char ** create_json_list(char* json_str)
{
        int i;
        char ** ret_list = (char**)malloc(login_struct.num_exp * sizeof(char*));
        for(i = 0; i < login_struct.num_exp; i++)
        {
                ret_list[i] = malloc(sizeof(char) * 1024);
        }

        //Find delimiting character to split into new string
        int list_i = -1; //Start at -1 because writing always starts with list_i++ at '{'
        int list_j = 0;

        for(i = 0; i < strlen(json_str); i++)
        {
                if(json_str[i] == '[' || json_str[i] == ']')
                        continue;
                if(json_str[i] == '{')
                {
                        list_i++;
                        list_j = 0;
                }
                ret_list[list_i][list_j] = json_str[i];

                if(json_str[i] == '}')
                {
                        i++;
                        ret_list[list_i][list_j+1] = 0; //Null terminate
                        i++;
                }
                list_j++;
        }

        return ret_list;
}


void parseInit(char* jsonString)
{
    cJSON *root =                       cJSON_Parse(jsonString);                        // Read the JSON
    cJSON *ip =                         cJSON_GetObjectItem(root,"ip");              // Search for the key "ip"
    cJSON *subnet_mask =                cJSON_GetObjectItem(root,"subnet_mask");              // Search for the key "subnet_mask"
    cJSON *gateway =                    cJSON_GetObjectItem(root,"gateway");              // Search for the key "gateway"
    cJSON *ssid =                       cJSON_GetObjectItem(root,"ssid");              // Search for the key "ssid"
    cJSON *pass =                       cJSON_GetObjectItem(root,"pass");              // Search for the key "pass"
    cJSON *num_exp =                    cJSON_GetObjectItem(root,"num_exp");              // Search for the key "num_exp"

    login_struct.ip = ip->valuestring;
    login_struct.subnet_mask = subnet_mask->valuestring;
    login_struct.gateway = gateway->valuestring;
    login_struct.ssid = ssid->valuestring;
    login_struct.pass = pass->valuestring;
    login_struct.num_exp = atoi(num_exp->valuestring);

    // Print to the screen to check
//    WPRINT_APP_INFO(("ip: %s\n", ip->valuestring)) ;
//    WPRINT_APP_INFO(("subnet_mask: %s\n", subnet_mask->valuestring)) ;
//    WPRINT_APP_INFO(("gateway: %s\n", gateway->valuestring)) ;
//    WPRINT_APP_INFO(("ssid: %s\n", ssid->valuestring)) ;
//    WPRINT_APP_INFO(("pass: %s\n", pass->valuestring)) ;
//    WPRINT_APP_INFO(("num_exp: %d\n", num_exp->valueint)) ;
//
//    // Print to the screen to check if params are properly set
//    WPRINT_APP_INFO(("testinit.ip: %s\n", ip->valuestring)) ;
//    WPRINT_APP_INFO(("testinit.subnet_mask: %s\n", subnet_mask->valuestring)) ;
//    WPRINT_APP_INFO(("testinit.gateway: %s\n", gateway->valuestring)) ;
//    WPRINT_APP_INFO(("testinit.ssid: %s\n", ssid->valuestring)) ;
//    WPRINT_APP_INFO(("testinit.pass: %s\n", pass->valuestring)) ;
//    WPRINT_APP_INFO(("testinit.num_exp: %d\n", num_exp->valueint)) ;
}

/* Expects an array of jsonStrings that should equal login_struct */
void parseExperiments(char** jsonString)
{
    int i = 0;
    while (i < login_struct.num_exp)
    {
        cJSON *root =                       cJSON_Parse(jsonString[i]);                        // Read the JSON
        cJSON *target_ip =                  cJSON_GetObjectItem(root, "target_ip");                     //IP to send packets to in burst
        cJSON *burst_num =                  cJSON_GetObjectItem(root,"burst_num");              // Search for the key "burst_num"
        cJSON *packet_num =                 cJSON_GetObjectItem(root,"packet_num");              // Search for the key "packet_num"
        cJSON *transport_protocol =         cJSON_GetObjectItem(root,"transport_protocol");              // Search for the key "transport_protocol"
        cJSON *tos =                        cJSON_GetObjectItem(root,"tos");              // Search for the key "tos"
        cJSON *ack =                        cJSON_GetObjectItem(root,"ack");              // Search for the key "ack"
        cJSON *delay_burst =                cJSON_GetObjectItem(root,"delay_burst");              // Search for the key "delay_burst"
        cJSON *delay_exp =                  cJSON_GetObjectItem(root,"delay_exp");              // Search for the key "delay_exp"
        cJSON *size_msg =                   cJSON_GetObjectItem(root,"size_msg");              // Search for the key "size_msg"
        cJSON *psm =                        cJSON_GetObjectItem(root,"psm");              // Search for the key "psm"

        //initializing test struct
        test[i].target_ip = malloc(sizeof(char) * strlen(target_ip->valuestring) + 1);
        strcpy(test[i].target_ip, &((*target_ip).valuestring[0]));

        test[i].num_burst = atoi(burst_num->valuestring);
        test[i].packets_per_burst = atoi(packet_num->valuestring);

        if (strcmp(transport_protocol->valuestring, "tcp") == 0) {
            test[i].tcp_udp = 0;
        }
        else if (strcmp(transport_protocol->valuestring, "udp") == 0) {
            test[i].tcp_udp = 1;
        }

        if (strcmp(tos->valuestring, "Best effort") == 0) {
            test[i].type_of_service = 0x00;
        }
        else if (strcmp(tos->valuestring, "Background") == 0) {
            test[i].type_of_service = 0x40;
        }
        else if (strcmp(tos->valuestring, "Video") == 0) {
            test[i].type_of_service = 0x20;
        }
        if (strcmp(ack->valuestring, "false") == 0) {
            test[i].ack_enable_disable = 0;
        }
        else if (strcmp(ack->valuestring, "true") == 0) {
            test[i].ack_enable_disable = 1;
        }
        test[i].burst_delay = atoi(delay_burst->valuestring);
        test[i].exp_delay = atoi(delay_exp->valuestring);
        test[i].junkMessageSize = atoi(size_msg->valuestring);

        if (strcmp(psm->valuestring, "false") == 0) {
            test[i].powerSaveMode = 0;
        }
        else if (strcmp(psm->valuestring, "true") == 0) {
            test[i].powerSaveMode = 1;
        }

//        WPRINT_APP_INFO(("%s \n", jsonString[i]));
//        WPRINT_APP_INFO(("\n"));
//
//        // Print to the screen to make sure the parameters were properly set
        WPRINT_APP_INFO(("test[%d].target_ip: %s\n", i, test[i].target_ip));
//        WPRINT_APP_INFO(("test[%d].num_burst: %d\n", i, test[i].num_burst));
//        WPRINT_APP_INFO(("test[%d].packets_per_burst: %d\n", i, test[i].packets_per_burst));
//        WPRINT_APP_INFO(("test[%d].tcp_udp: %d\n", i, test[i].tcp_udp));
//        WPRINT_APP_INFO(("test[%d].type_of_service: 0x%02X\n", i, test[i].type_of_service));
//        WPRINT_APP_INFO(("test[%d].ack_enable_disable: %d\n", i, test[i].ack_enable_disable));
//        WPRINT_APP_INFO(("test[%d].burst_delay: %d\n", i, test[i].burst_delay));
//        WPRINT_APP_INFO(("test[%d].exp_delay: %d\n", i, test[i].exp_delay));
//        WPRINT_APP_INFO(("test[%d].junkMessageSize: %d\n", i, test[i].junkMessageSize));
//        WPRINT_APP_INFO(("test[%d].powerSaveMode: %d\n", i, test[i].powerSaveMode));
//        WPRINT_APP_INFO(("\n"));
        i++;
    }
}


void sendData(uint32_t expNum, uint32_t burstNum, char* sendMessage, uint32_t pkt_per_burst, uint8_t packet)
{
    wiced_packet_t* tx_packet;
    uint8_t *tx_data;
    uint16_t available_data_length;
    wiced_result_t result;
    uint32_t counter = 0;


    double rssi_average = 0;
    double rate_average = 0;
    double noise_average = 0;

//    double rssi_average1[burstNum];
//    double rate_average1[burstNum];
//    double noise_average1[burstNum];


    // Create the packet and send the data
    while (counter < pkt_per_burst)
    {
        rssi_val = 0;
        rate_val = 0;
        noise_val = 0;
        time_val = 0;

       // WPRINT_APP_INFO(("Packet #: %d\n", z+1));
//       wwd_wifi_get_rssi(&rssi_val);      // RSSI
//       wwd_wifi_get_rate(0, &rate_val);   // Rate
//       wwd_wifi_get_noise(&noise_val);    // Noise
//       wiced_time_get_time(&time_val);    // Time


       //get the total of each statistic
//       rssi_average += rssi_val;
//       rate_average += rate_val;
//       noise_average += noise_val;


       //wwd_wifi_get_listen_interval(&test[expNum].li); //Listen Interval--should be 0 if default

        // format the sendMessage packet to includes Junk Message | RSSI | Rate | Noise | Time | TCP or UDP | Experiment Number | Burst Number | Packet Number
        sprintf(&sendMessage[test[expNum].junkMessageSize+1], ",%.1f,%.1f,%.1f,%u,%d,%04x,%04x,%04x!", (double) rssi_val, (double) rate_val, (double) noise_val, (unsigned int) time_val, (unsigned) test[expNum].tcp_udp, (unsigned) expNum, (unsigned) burstNum, (unsigned) counter);

        // Create the packet and send the data
        if(packet == TCP)
            // create tcp packet
            wiced_packet_create_tcp(&tcp_socket, strlen(sendMessage), &tx_packet, (uint8_t**)&tx_data, &available_data_length); // get a TCP packet
        else
            // create udp packet
            wiced_packet_create_udp(&udp_socket, strlen(sendMessage), &tx_packet, (uint8_t**)&tx_data, &available_data_length); // get a UDP packet


        memcpy(tx_data, sendMessage, strlen(sendMessage)); // put our data in the packet
        wiced_packet_set_data_end(tx_packet, (uint8_t*)&tx_data[strlen(sendMessage)]); // set the end of the packet

        if(packet == TCP)
            wiced_tcp_send_packet(&tcp_socket, tx_packet);
        else
            wiced_udp_send(&udp_socket, &serverAddress, SERVER_PORT, tx_packet);

#ifdef verbose
//        WPRINT_APP_INFO(("Sending...Done!\n"));
#endif

        counter++;
    }
    //get the average of each statistic per burst
    rssi_average = rssi_average/pkt_per_burst;
    rate_average = rate_average/pkt_per_burst;
    noise_average = noise_average/pkt_per_burst;

//    char sendStatMsg[100];

//    WPRINT_APP_INFO(("Testing\n"));
//    WPRINT_APP_INFO(("!%d,%d,%.1f,%.1f,%.1f", (unsigned) expNum, (unsigned) burstNum, rssi_average, rate_average, noise_average));
//    WPRINT_APP_INFO((",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d!", EXP_NUM, BURST, PACKETS_PER_BURST, EXP_DELAY, BURST_DELAY, TCP, UDP, BROADCAST, UNICAST, TX_POWER));
//    WPRINT_APP_INFO(("\n"));
//    sprintf(&sendStatMsg[0], "!%d,%d,%.1f,%.1f,%.1f", (unsigned) expNum, (unsigned) burstNum, rssi_average, rate_average, noise_average);
//    WPRINT_APP_INFO(("First half of statistics message to send\n"));
//    WPRINT_APP_INFO(("%s\n",sendStatMsg));
//    sendStatistics(sendStatMsg, packet);

//    rssi_average1[burstNum] = rssi_average;
//    rate_average1[burstNum] = rate_average;
//    noise_average1[burstNum] = noise_average;

//    int inc = 0;
//    while (inc < burstNum) {
//        WPRINT_APP_INFO(("RSSI array is:"));
//        WPRINT_APP_INFO((" %.1f ", rssi_average1[burstNum]));
//        inc++;
//    }

    wiced_packet_delete(tx_packet);

    // Get the response back from the WWEP server--deleting this led to problems..why?
    wiced_packet_t *rx_packet;

    if (packet == TCP)
        result = wiced_tcp_receive(&tcp_socket, &rx_packet, 1); // wait up to 1ms for response
    else
        result = wiced_udp_receive(&udp_socket, &rx_packet, 1); // wait up to 1ms for response

    wiced_packet_delete(rx_packet);
}

//void sendStatistics(char* sendStatisticsMessage, double rssi_average, double rate_average, double noise_average, uint8_t packet)
//void sendStatistics(char* sendStatisticsMessage, uint8_t packet)
//{
//    wiced_packet_t* tx_packet;
//    uint8_t *tx_data;
//    uint16_t available_data_length;
//    wiced_result_t result;
//
//       // Create the packet and send the data
//   //delete later
//    char temp[50];
//
//   #ifdef verbose
//           WPRINT_APP_INFO(("Sending packet of statistics\n"));
//   #endif
//           // format the sendStatisticsMessage packet
//           sprintf(&temp[0], ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d!", EXP_NUM, BURST, PACKETS_PER_BURST, EXP_DELAY, BURST_DELAY, TCP, UDP, BROADCAST, UNICAST, TX_POWER);
//           //need to concatenate two strings
//           //strcat(sendStatisticsMessage, )
//           WPRINT_APP_INFO(("Testing temp statistics message %s\n", temp));
//           sprintf(&sendStatisticsMessage[27], ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d!", EXP_NUM, BURST, PACKETS_PER_BURST, EXP_DELAY, BURST_DELAY, TCP, UDP, BROADCAST, UNICAST, TX_POWER);
//           //iterate through the arrays of rssi, rate, and noise and add those to the end of the sendStatistics message
//
//
//   #ifdef verbose
//           WPRINT_APP_INFO(("Entire statistics message\n",sendStatisticsMessage));
//           WPRINT_APP_INFO(("%s\n",sendStatisticsMessage)); // echo the message so that the user can see what they're sending
//   #endif
//
//           // Create the packet and send the data
//           if(packet == TCP)
//               // create tcp packet
//               wiced_packet_create_tcp(&tcp_socket, strlen(sendStatisticsMessage), &tx_packet, (uint8_t**)&tx_data, &available_data_length); // get a TCP packet
//           else
//               // create udp packet
//               wiced_packet_create_udp(&udp_socket, strlen(sendStatisticsMessage), &tx_packet, (uint8_t**)&tx_data, &available_data_length); // get a UDP packet
//
//
//           memcpy(tx_data, sendStatisticsMessage, strlen(sendStatisticsMessage)); // put our data in the packet
//           wiced_packet_set_data_end(tx_packet, (uint8_t*)&tx_data[strlen(sendStatisticsMessage)]); // set the end of the packet
//
//
//   #ifdef verbose
//           WPRINT_APP_INFO(("Sending...\n"));
//   #endif
//
//           if(packet == TCP)
//               wiced_tcp_send_packet(&tcp_socket, tx_packet);
//           else
//               wiced_udp_send(&udp_socket, &serverAddress, SERVER_PORT, tx_packet);
//
//   #ifdef verbose
//           WPRINT_APP_INFO(("Sending...Done Sending Statistics Message!\n"));
//   #endif
//
//       wiced_packet_delete(tx_packet);
//
//       // Get the response back from the WWEP server--deleting this led to problems..why?
//       wiced_packet_t *rx_packet1;
//
//       if (packet == TCP)
//           result = wiced_tcp_receive(&tcp_socket, &rx_packet1, 1); // wait up to 1ms for response
//       else
//           result = wiced_udp_receive(&udp_socket, &rx_packet1, 1); // wait up to 1ms for response
//
//       wiced_packet_delete(rx_packet1);
//}


//Function creates socket according to type (TCP/UDP)
void create_socket (int socket_type, uint32_t tos)
{
    if(socket_type == TCP) //create TCP socket
    {
        wiced_tcp_create_socket(&tcp_socket, WICED_STA_INTERFACE);

        // The type of service, where 0x00 or 0xC0 = Best effort, 0x40 or 0x80 = Background, 0x20 or 0xA0 = Video, 0x60 or 0xE0 = Voice
        // *** We have three options for the library used: LwIP, NetX, NetX Duo
        // Use, for example, ...-FreeRTOS-LwIP ...
        wiced_tcp_set_type_of_service(&tcp_socket, tos);
        wiced_tcp_bind(&tcp_socket, WICED_ANY_PORT);
        wiced_tcp_connect(&tcp_socket,&serverAddress, SERVER_PORT, 1); // 1 millisecond timeout
    }
    else
    {
        wiced_udp_create_socket(&udp_socket, SERVER_PORT, WICED_STA_INTERFACE);
        wiced_udp_set_type_of_service(&udp_socket, tos);
    }
}

//Function deletes socket
void delete_socket(int socket_type)
{
    if(socket_type==TCP)
    {
        wiced_tcp_delete_socket(&tcp_socket);    // Delete the TCP socket
    }
    else
    {
        wiced_udp_delete_socket(&udp_socket);    // Delete the UDP socket
    }
}

//Function sets junk message
void set_junk_message(char *msg, int length)
{
    msg[0]  ='!';
    msg[1]  ='#';
    memset(msg+2, '.',length-1);
    msg[length] = '#';
}

/*Todo: Make sure that broadcast_address is not hardcoded, or reset it yourself for future */
//Function sets ip address according to type
void set_server_ip_address(uint8_t type, const char* target_ip)
{
    wiced_result_t result;
//    int str_to_ip( const char* arg, wiced_ip_address_t* address );
    str_to_ip(target_ip, &serverAddress);

//    SET_IPV4_ADDRESS( serverAddress, MAKE_IPV4_ADDRESS(server_address[0],server_address[1], server_address[2], server_address[3]) ); //broadcast address

//    if(type == BROADCAST)
//    {
//        SET_IPV4_ADDRESS( serverAddress, MAKE_IPV4_ADDRESS(broadcast_address[0],broadcast_address[1], broadcast_address[2], broadcast_address[3]) ); //broadcast address
//
//#ifdef verbose
//        WPRINT_APP_INFO(("Broadcast Address set\n"));
//#endif
//
//    }
//    else
//    {
        // !! Commented out since login config should be setting serverAddress

//        result = wiced_hostname_lookup("raspberrypi", &serverAddress, 1, WICED_STA_INTERFACE);
//        if (result == WICED_ERROR || serverAddress.ip.v4 == 0)
//        {
//            SET_IPV4_ADDRESS( serverAddress, MAKE_IPV4_ADDRESS( server_address[0],server_address[1], server_address[2], server_address[3]));
////            WPRINT_APP_INFO(("Raspberry Pi Hardcoded Address set\n"));
//        }
//    }
}


/*
packetSendThreadMain:
   This function is the thread that sends the packets to the server via the sendTCPData/sendUDPData function.
   Right now, it waits for button presses.
 */
void packetSendThreadMain()
{
    // Main Loop: Send the data
    uint32_t i = 0;
    while (i < login_struct.num_exp)   //exp
    {
        if (test[i].powerSaveMode == 1)
            wiced_wifi_enable_powersave();
        else if (test[i].powerSaveMode == 2)
        {
            const uint8_t return_to_sleep_delay = 10;
            wiced_wifi_enable_powersave_with_throughput( return_to_sleep_delay );
        }

        //This is the junk message inside the send message, which is a character array: #.......# of length 1,300
        char sMessage[test[i].junkMessageSize + 60 + 1];
        set_junk_message(sMessage, test[i].junkMessageSize);
        //Todo: Null terminate after junkMessageSize?

        set_server_ip_address(test[i].ack_enable_disable, test[i].target_ip); // Todo: Currently only sets to hardcoded broadcast if enabled

        //set listen interval at the beginning of each experiment
        wiced_wifi_set_listen_interval(test[i].li, WICED_LISTEN_INTERVAL_TIME_UNIT_BEACON);

        uint32_t j = 0;
        while (j < test[i].num_burst)  //bursts
        {
            WPRINT_APP_INFO(("Running burst: %d\n", j)); //TODO: Temporary
            j++;
            wiced_rtos_delay_milliseconds(test[i].burst_delay); //delay between bursts

            create_socket(test[i].tcp_udp, test[i].type_of_service); //Todo: Uses udp_socket or tcp_socket which may be hardcoded, check init
            sendData(i, j, sMessage, test[i].packets_per_burst,test[i].tcp_udp); // Todo: Try garbage here first
            delete_socket(test[i].tcp_udp);
        }

        wiced_rtos_delay_milliseconds(test[i].exp_delay); //time interval of experiments
        i++;
    }
    WPRINT_APP_INFO( ("STOP\n") );
}

//Currently writing to dct causes errors,
//Writing to dct_local causes network_up to crash
void set_login_info(platform_dct_wifi_config_t * wifi_config_dct_local)
{
    WPRINT_APP_INFO( ("----!!!Retrieved SSID: %s & pass: %s\n", login_struct.ssid, login_struct.pass) );

    if(wiced_dct_read_with_copy( wifi_config_dct_local, DCT_WIFI_CONFIG_SECTION, 0, sizeof(platform_dct_wifi_config_t) ) == WICED_SUCCESS)
    {
//        WPRINT_APP_INFO( ("Read success...\n") );
    }
    else
    {
        WPRINT_APP_INFO( ("Read failure!...") );
    }

    wiced_scan_result_t * ap_results = malloc(sizeof(wiced_scan_result_t));
//    snprintf(&ssid_str[0], 15, "SIOTLAB-224-2G");

    wiced_wifi_find_ap(login_struct.ssid, ap_results, NULL);
//    WPRINT_APP_INFO( ("found SSID: %s\n", ap_results->SSID.value) );

    strncpy((char*)(wifi_config_dct_local)->stored_ap_list[0].details.SSID.value, ap_results->SSID.value, strlen(ap_results->SSID.value));
    wifi_config_dct_local->stored_ap_list[0].details.SSID.length = strlen(ap_results->SSID.value);
//    WPRINT_APP_INFO( ("Newly Set SSID: %s\n", wifi_config_dct_local->stored_ap_list[0].details.SSID.value) );

    free(ap_results);

//            /* Set passcode */
//    sprintf(pass_str, "research-lab-ap-cr");
    memcpy((char*)(wifi_config_dct_local)->stored_ap_list[0].security_key, login_struct.pass, strlen(login_struct.pass));
    wifi_config_dct_local->stored_ap_list[0].security_key_length = strlen(login_struct.pass);
//    WPRINT_APP_INFO( ("Newly Set Passcode: %s\n", wifi_config_dct_local->stored_ap_list[0].security_key) );

//    return wifi_config_dct_local;
}

/* Assumes that json_str has already been parsed into login_struct */
void ip_config_setup(wiced_ip_setting_t * ip_config)
{
    //! Make sure these aren't pass by refrence
    /* Set Server IP */
//    const char * temp_ip = "192.168.69.69";
//    WPRINT_APP_INFO( ("Read temp_ip as %s\n", login_struct.ip) );
    str_to_ip( login_struct.ip, &serverAddress );
//    WPRINT_APP_INFO( ("Set Server Address!\n") );

    //What ip address is this?
//    WPRINT_APP_INFO( ("Read temp_ip as %s\n", login_struct.ip) );
    str_to_ip(login_struct.ip, &(*ip_config).ip_address);
//    WPRINT_APP_INFO( ("Set Server IP!\n") );

    /* Set netmask */
//    const char * temp_netmask = "255.255.255.255";
//    WPRINT_APP_INFO( ("Read temp_ip as %s\n", login_struct.subnet_mask) );
    str_to_ip(login_struct.subnet_mask, &(*ip_config).netmask);
//    WPRINT_APP_INFO( ("Set netmask!\n") );

//    const char * temp_gateway = "192.168.2.1";
//    WPRINT_APP_INFO( ("Read temp_ip as %s\n", login_struct.gateway) );
    str_to_ip(login_struct.gateway, &(*ip_config).gateway);
//    WPRINT_APP_INFO( ("Set Gateway!\n") );

}


/*
 * Meant to be a blocking call to set up settings, called from packetSendThreadMain,
 * called at the end of app_start
 * Printing STOP will the only thing that signals server to finish waiting form more messages
 *   It is an asynchronous terminator
 */
void start_state_machine()
{
    char c;
    uint32_t expected_data_size = 1;

    int state = 0;
    WPRINT_APP_INFO( ("Starting Continuous loop!\n") );
    while(1)
    {

        memset(buf, 0, expected_data_size);
        wiced_uart_receive_bytes(STDIO_UART, &buf, &buf_size, WICED_NEVER_TIMEOUT);
        buf_size = 4096;
        /* *** Chop String *** */
        int i;
        for(i = 0; i < buf_size; i++)
        {
            if(buf[i] == '*')
                break;
        }

        int char_state = buf[i-1] - '0'; //i is the
        buf[i-1] = 0; //null terminate before meta data

        WPRINT_APP_INFO( ("Msg: %s\n", buf) );
        WPRINT_APP_INFO( ("State: %d\n", char_state) );

        //Begin to set network parameters
        if(char_state == 0)
        {
            platform_dct_wifi_config_t *wifi_config_dct_local = malloc(sizeof(platform_dct_wifi_config_t));
            wiced_ip_setting_t *ip_config = malloc(sizeof(wiced_ip_setting_t));

            /* Parse json init beforehand -- prerequisite for set_login_info() and ip_config_setup  */
            parseInit(buf);

            /* Set custom ssid */
            set_login_info(wifi_config_dct_local); //Requires parseInit() was already called
            wiced_result_t dct_res = wiced_dct_write ( (const void *)wifi_config_dct_local, DCT_WIFI_CONFIG_SECTION, 0, sizeof (platform_dct_wifi_config_t));
            free(wifi_config_dct_local);

            ip_config_setup(ip_config); //Requires parseInit() was already called

            wiced_result_t up_ret = wiced_network_up( WICED_STA_INTERFACE, WICED_USE_STATIC_IP, ip_config );
            if(up_ret == WICED_ERROR)
            {
                WPRINT_APP_INFO( ("An error occured in network_up!\n") );
            }

            free(ip_config);
            WPRINT_APP_INFO( ("STOP\n") );
        }

        //Set and run experiment configurations
        else if(char_state == 1)
        {
            WPRINT_APP_INFO( ("Packet Config State...\n") );

            char ** json_list = create_json_list(buf);
            WPRINT_APP_INFO( ("Created json_list...\n") );
            parseExperiments(json_list);
            free(json_list); //Free json list after struct has been configured
            WPRINT_APP_INFO( ("Experiment Configuration Complete\n") );

            //Call thread using new parameters
            wiced_rtos_delete_thread(&packetSendThread);
            wiced_rtos_create_thread(&packetSendThread, PACKETSENDTHREADPRIORITY, "packetSend Thread", packetSendThreadMain, TCP_CLIENT_STACK_SIZE, 0);
            WPRINT_APP_INFO( ("Completed sending packets!\n") );
        }

        //Received first message
        else if(char_state == 2)
        {
            //This needs to run on new thread
            WPRINT_APP_INFO( ("About to deliberately crash program to powercycle board\n") );
            WPRINT_APP_INFO( ("STOP\n") );
            char *p = NULL;
            *p = 'a'; //Dereference will throw null pointer exception which should restart board
        }
    }
//    free(buf);
}


/* This works as a test */
void application_start(void)
{
    /*Initializes the device and WICED framework */
    wiced_init();

    wiced_time_set_time(&time_val);

    /* Configure uart */
    ring_buffer_init(&rx_buffer, rx_data, 64 );
    wiced_uart_init( STDIO_UART, &uart_config, &rx_buffer );

    start_state_machine();
}
