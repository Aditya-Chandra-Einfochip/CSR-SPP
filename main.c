#include <message.h>
#include <connection.h>
#include <sink.h>
#include <source.h>
#include <stream.h>
#include <panic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>  /* For debug print*/
#include "spps.h"
#include "spp_common.h"
#include "power.h"
#include "connection.h"

#define SPP_MAX_PAYLOAD 256  /* Maximum SPP message buffer size*/
#define MAX_N_BYTE_DATA 512  /* Maximum arbitrary data transfer size */
static void sppStartServer(void);
typedef struct
{
    TaskData task;
    SPP *spp;
    Sink sink;
    char buffer[SPP_MAX_PAYLOAD];  /* Buffer for incoming data*/
    uint16 buf_len;                /* Length of buffered data*/
} SppAppData;

static SppAppData spp_app;
static void sppSendNBytes(Sink sink, const char *, uint16);
/* Function to send data over SPP */
static void sppSendData(Sink sink, const char *data, uint16 size)
{
    if (sink && size > 0)
    {
        uint8 *sink_data = SinkMap(sink);
        if (sink_data)
        {
            memcpy(sink_data, data, size);
            SinkFlush(sink, size);
        }
    }
}



/* Function to handle incoming SPP data */
static void sppHandleIncomingData(SppAppData *app)
{
    Source source = StreamSourceFromSink(app->sink);
    uint16 size = SourceSize(source);
    
    if (size > 0)
    {
        const uint8 *received_data = SourceMap(source);
        
        /* Append new data to buffer*/
        if (app->buf_len + size < SPP_MAX_PAYLOAD)
        {
            memcpy(app->buffer + app->buf_len, received_data, size);
            app->buf_len += size;
        }

        /* Check for "\r\n" sequence*/
        if (app->buf_len >= 2 &&
            app->buffer[app->buf_len - 2] == '\r' &&
            app->buffer[app->buf_len - 1] == '\n')
        {
            
            app->buffer[app->buf_len] = '\0';

            /* Echo the received message back*/
            sppSendNBytes(app->sink, app->buffer, app->buf_len);

            /* Reset buffer for next message */
            app->buf_len = 0;
        }

        /* Drop the received data to clear the buffer*/
        SourceDrop(source, size);
    }
}
static void sppSendNBytes(Sink sink, const char *data, uint16 size)
{
    if (size > MAX_N_BYTE_DATA)
    {
        size = MAX_N_BYTE_DATA;  /* Limit data to max transfer size*/
    }
     sppSendData(sink, data, size);
 }
/* SPP event handler */
static void sppMessageHandler(Task task, MessageId id, Message message)
{
    switch (id)
    {
        case CL_INIT_CFM:
        {
            CL_INIT_CFM_T *msg = (CL_INIT_CFM_T *)message;
            if (msg->status == success)
            {
                printf("Bluetooth Stack Initialized Successfully\n");
               ConnectionDmBleSetScanEnable(TRUE);  /* Start SPP after Bluetooth is ready*/
            }
            else
            {
                printf("Bluetooth Stack Initialization Failed\n");
                Panic();
            }
        }
        break;

        case SPP_SERVER_CONNECT_CFM:
        {
            SPP_SERVER_CONNECT_CFM_T *msg = (SPP_SERVER_CONNECT_CFM_T *)message;
            if (msg->status == spp_connect_success)
            {
                spp_app.sink = msg->sink;
                MessageSinkTask(spp_app.sink, &spp_app.task);
                printf("SPP Connection Established!\n");
            }
            else
            {
                printf("SPP Connection Failed!\n");
            }
        }
        break;

        case SPP_MESSAGE_MORE_DATA:
            sppHandleIncomingData(&spp_app);
            break;

        case SPP_DISCONNECT_IND:
            spp_app.sink = NULL; /* Clear sink on disconnect*/
            printf("Device Disconnected!\n");
            break;

        default:
            break;
    }
}

/* Function to initialize SPP server */
static void sppStartServer(void)
{
    spp_app.task.handler = sppMessageHandler;
    spp_app.buf_len = 0;
    printf("Starting SPP Server...\n");
    SppStartService(&spp_app.task);
}

/* Main function */
int main(void)
{   
   
    ConnectionInit(&spp_app.task);  /* Initialize Bluetooth Connection*/
    printf("Waiting for Bluetooth Ready...\n");
    sppStartServer();
      
   
    MessageLoop();  /* Run message loop (never returns)*/

    return 0;
    
}



