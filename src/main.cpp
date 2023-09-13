/*
 *
 *
 *
 * https://medium.com/@fatehsali517/how-to-connect-esp32-to-wifi-using-esp-idf-iot-development-framework-d798dc89f0d6
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html
 * https://esp32tutorials.com/esp32-esp-idf-get-set-custom-mac-address/
 * https://www.eevblog.com/forum/microcontrollers/print-ip-from-esp32-wroom-with-esp-edf-in-station-mode/
 */



#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "cred.h"

//#include <stdio.h> //for basic printf commands
//#include <string.h> //for handling strings
//#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "esp_system.h" //esp_init funtions esp_err_t 
//#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_log.h" //for showing logs
//#include "esp_event.h" //for wifi event
#include "nvs_flash.h" // Libraries for Non Volatile Storage
#include "lwip/err.h" //light weight ip packets error handling
#include "lwip/sys.h" //system applications for light weight ip apps

//#include "esp_err.h" // Error types for grabbing data functions

extern "C" {
  void app_main();
}

// Gross Waiting flag glue
static bool is_wifi_connected = false;

/*
 * ##################################################################
 * GPIO Module
 * ##################################################################
 */

#define GPIO_LED  GPIO_NUM_4


/*
 * ##################################################################
 * WiFi Module
 * ##################################################################
 */

/*
 * Enter your WiFi credentials
 */
const char* ssid = my_ssid;
const char* password = my_pass;

int retry_num = 5;

void print_mac()
{
    printf("Entering print_mac()...\n");

    #define MAC_ADDR_SIZE 6
    uint8_t mac[MAC_ADDR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF, 0xBA, 0xBE};

    esp_err_t err = ERR_ARG;
    err = esp_wifi_get_mac(WIFI_IF_STA, mac);

    if (err == ERR_OK)
    {
        printf("STA MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else
    {
        printf("Failed to get MAC Address\n");
    }
}

void print_auth_mode()
{
    printf("Entering print_auth_mode()...\n");

    wifi_config_t config_info;

    esp_err_t err = ERR_ARG;
    err = esp_wifi_get_config(WIFI_IF_STA, &config_info);

    if (err == ERR_OK)
    {
        wifi_auth_mode_t auth_mode = config_info.sta.threshold.authmode;

        switch (auth_mode) {
            case WIFI_AUTH_OPEN:
                printf("Wi-Fi Security Mode: Open (No security)\n");
                break;
            case WIFI_AUTH_WEP:
                printf("Wi-Fi Security Mode: WEP\n");
                break;
            case WIFI_AUTH_WPA_PSK:
                printf("Wi-Fi Security Mode: WPA-PSK (WPA2-PSK)\n");
                break;
            case WIFI_AUTH_WPA2_PSK:
                printf("Wi-Fi Security Mode: WPA2-PSK\n");
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                printf("Wi-Fi Security Mode: WPA/WPA2-PSK\n");
                break;
            default:
                printf("Wi-Fi Security Mode: Unknown\n");
                break;
        }
    } else
    {
        printf("Failed to get Wi-Fi configuration\n");
    }
}

void print_ip()
{
    //while(1)
    {
        printf("Entering print_ip()...\n");

        esp_netif_ip_info_t ip_info;

        esp_netif_t* netif = NULL;
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

        if (netif == NULL)
            printf("Pointer is NULL.\n");
        else
        {
            // If we got a pointer, show the address.
            printf("Pointer is: 0x%" PRIXPTR "\n", (uintptr_t)netif);

            // Copy the netif IP info into our variable.
            esp_netif_get_ip_info(netif, &ip_info);

            printf("My IP: " IPSTR "\n", IP2STR(&ip_info.ip));
            printf("My GW: " IPSTR "\n", IP2STR(&ip_info.gw));
            printf("My NM: " IPSTR "\n", IP2STR(&ip_info.netmask));
        }
       
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
        case (WIFI_EVENT_STA_START):
        {
            printf("WIFI CONNECTING....\n");
        } break;

        case (WIFI_EVENT_STA_CONNECTED):
        {
            printf("WIFI CONNECTED\n");
        } break;

        case (WIFI_EVENT_STA_DISCONNECTED):
        {
            printf("WIFI lost connection\n");
            if (retry_num < 5)
            {
                esp_wifi_connect();
                retry_num++;
                printf("Retrying to Connect...  Retry #%i...\n", retry_num);
            }
        } break;

        case (IP_EVENT_STA_GOT_IP):
        {
            printf("WIFI got IP...\n\n");
            is_wifi_connected = true;
            print_ip();
        } break;

    }
}

void wifi_connection()
{
    printf("Start of wifi_connection()\n");
    // 1. Wi-Fi Init Phase
    printf("Start of 1. Wi-Fi Init Phase\n");
    esp_netif_init();   // Initialize TCP/IP stack
    esp_event_loop_create_default();    // Responsible for handling and dispatching events
    esp_netif_create_default_wifi_sta();    // Sets up neccesary data structures for Wifi STA
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();    // Sets default values
    esp_wifi_init(&wifi_initiation);    // Initialize Wifi driver with default init values

    // Event Handling
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);   //
        // Create (register) an event handler for Wifi
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);    //
        // Create (register) an event handler for Wifi

    // 2. Wi-Fi Configuration Phase 
    printf("Start of 2. Wi-Fi Configuration Phase \n");
        // Suppress the warning about uninitialized members in wifi_config_t
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = ""
        }
    };
        #pragma GCC diagnostic pop
    strcpy( (char*)wifi_configuration.sta.ssid, ssid );
    strcpy( (char*)wifi_configuration.sta.password, password );
    esp_wifi_set_config(WIFI_IF_STA, &wifi_configuration);  // Setting up wifi config as Station
        // Stores values in NVS

    // 3. Wi-Fi Start Phase
    printf("Start of 3. Wi-Fi Start Phase\n");
    esp_wifi_start();   // Start connection with provided intitalizations
    esp_wifi_set_mode(WIFI_MODE_STA);   // Station mode chosen

    // 4. Wi-Fi Connect Phase
    printf("Start of 4. Wi-Fi Connect Phase\n");
    esp_wifi_connect(); // Will scan and connect to chosen SSID

    printf("Wifi Station Conection complete.\n  SSID: %s  Password: %c%c**\n\n", ssid, password[0], password[1]);

    printf("End of wifi_connection() function 1/2.\n");
    print_mac();
    print_ip();
    print_auth_mode();
    printf("End of wifi_connection() function 2/2.\n");
}

void wifi_disconnection()
{
    esp_wifi_disconnect();  // Disconnects Wifi connectivity
    esp_wifi_stop();    // Stops the Wifi driver
    esp_wifi_deinit();  // Unloads the Wifi driver
}

/*
 * ##################################################################
 * HTTP Module
 * ##################################################################
 */

#define MAX_CONTENT_LENGTH 4
const char* url = "http://www.angelfire.com/scifi/tim1/esp.png";

esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    switch (event->event_id)
    {
        case HTTP_EVENT_ERROR:
            printf("HTTP_EVENT_ERROR\n");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            printf("HTTP_EVENT_ON_CONNECTED\n");
            break;
        case HTTP_EVENT_HEADER_SENT:
            printf("HTTP_EVENT_HEADER_SENT\n");
            break;
        case HTTP_EVENT_ON_HEADER:
            printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s\n", event->header_key, event->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            printf("HTTP_EVENT_ON_DATA, len=%d\n", event->data_len);
            // Process the received data here
            // You can store it in a buffer and truncate it to the desired length (e.g., 10 bytes)
            break;
        case HTTP_EVENT_ON_FINISH:
            printf("HTTP_EVENT_ON_FINISH\n");
            break;
        case HTTP_EVENT_DISCONNECTED:
            printf("HTTP_EVENT_DISCONNECTED\n");
            break;
        case HTTP_EVENT_REDIRECT:
            printf("HTTP_EVENT_REDIRECT\n");
            break;
    }
    return ESP_OK;
}

char* make_http_request()
{
    printf("Start of make_http_request()\n");

        // Suppress the warning about uninitialized members in wifi_config_t
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_http_client_config_t client_config = {
        .url = url,
        .event_handler = http_event_handler
    };
        #pragma GCC diagnostic pop
    esp_http_client_handle_t http_client = esp_http_client_init(&client_config);

    esp_err_t err = esp_http_client_perform(http_client);

    if (err == ERR_OK)
    {
        // Success!
        // Prepare for incoming data
        char* data = (char*)malloc(MAX_CONTENT_LENGTH + 1);
        
        if (data)
        {
            // Upon successfull allocation:
            memset(data, 0, MAX_CONTENT_LENGTH + 1);
            int data_len = esp_http_client_read(http_client, data, MAX_CONTENT_LENGTH);

            if (data_len == -1) printf("Failed to client_read.\n");
            else printf("Successful client_read!\n");
            
            esp_http_client_cleanup(http_client);
            return data;
        } else
        {
            printf("Failed to allocate memory.\n");
        }

    } else
    {
        printf("HTTP Request Failed\n");
    }

    esp_http_client_cleanup(http_client);
    return NULL;
}


/*
 * ##################################################################
 * void app_main()
 * ##################################################################
 */

void app_main()
{
    printf("Start of app_main()\n");
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED, 0);


    nvs_flash_init();   // Non Volatile Storage Flash memory to store wifi configuration data

    wifi_connection();
    printf("Exited wifi_connection()\n");

    while (!is_wifi_connected)
    {
        // Gets stuck until wifi connects.
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    char* web_fetch = make_http_request();
    if (web_fetch)
    {
        printf("Inside web_fetch\n");
        // Use fetched Data
        for (int i = 0; i < MAX_CONTENT_LENGTH +1; i++)
        {
            printf("%c", web_fetch[i]);
        }
        printf("\n");

        free(web_fetch);
    }

    printf("End of app_main()\n");
}


