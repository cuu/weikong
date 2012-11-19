
#include <iocc2530.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OSAL.h"
#include "OSAL_Nv.h"

#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "ZDSecMgr.h"


#include "GenericApp.h"
#include "DebugTrace.h"

#if !defined( WIN32 )
  #include "OnBoard.h"
#endif

/* HAL */
#include "hal_adc.h"
#include "hal_mcu.h"
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"
#include "hal_drivers.h"

#include "nwk_util.h"

#include "mac_low_level.h"


/* RTOS */
#if defined( IAR_ARMCM3_LM )
#include "RTOS_App.h"
#endif  

//#define HAL_KEY_SW_6_BIT    BV(1)
//#define HAL_KEY_JOY_MOVE_BIT    BV(0)

// From hal_key.c 
extern uint8 FUCK_TI_KEY1; //
extern uint8 FUCK_TI_KEY7;
///-----------------------------


uint16 devlist[ NWK_MAX_DEVICES + 1];
uint8* ieeeAddr;
uint16 short_ddr;
uint16 panid;
uint8 global_start; // This is about a flag for after event STATE_CHANGE,clear statement for staring usage.

uint16 t3_counter;

// This list should be filled with Application specific Cluster IDs.
const cId_t GenericApp_ClusterList[GENERICAPP_MAX_CLUSTERS] =
{
  GENERICAPP_CLUSTERID
};


SimpleDescriptionFormat_t GenericApp_SimpleDesc =
{
  GENERICAPP_ENDPOINT,              //  int Endpoint;
  GENERICAPP_PROFID,                //  uint16 AppProfId[2];
  GENERICAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  GENERICAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  GENERICAPP_FLAGS,                 //  int   AppFlags:4;
  GENERICAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)GenericApp_ClusterList,  //  byte *pAppInClusterList;
  GENERICAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)GenericApp_ClusterList   //  byte *pAppInClusterList;
};

// This is the Endpoint/Interface description.  It is defined here, but
// filled-in in GenericApp_Init().  Another way to go would be to fill
// in the structure here and make it a "const" (in code space).  The
// way it's defined in this sample app it is define in RAM.
endPointDesc_t GenericApp_epDesc;


byte GenericApp_TaskID;   // Task ID for internal task/event processing
                          // This variable will be received when
                          // GenericApp_Init() is called.
devStates_t GenericApp_NwkState;


byte GenericApp_TransID;  // This is the unique message ID (counter)


devStates_t App_NwkState;

afAddrType_t GenericApp_DstAddr;


#define uint unsigned int
#define uchar unsigned char


#define SER_PORT 0

#define TEST_BUTTON P0_1

void Delay(unsigned  );
void UART_Send_String( unsigned char *,int );
void uprint( uint8*);

void serial_init(void );
void io_init(void);
void _halProcessKeyInterrupt (void);

void set_panid( uint16 );
uint8 cb_ReadConfiguration( uint8 , uint8 , void * );
uint8 cb_WriteConfiguration( uint8 , uint8 , void *);
uint8 SendData( uint16, uint8 *, uint8);

uint8 SendData( uint16 addr, uint8 *buf, uint8 Leng)
{
        afAddrType_t SendDataAddr;
        
        SendDataAddr.addrMode = (afAddrMode_t)Addr16Bit;
        SendDataAddr.endPoint = GENERICAPP_ENDPOINT;
        SendDataAddr.addr.shortAddr = addr;
        if ( AF_DataRequest( &SendDataAddr, &GenericApp_epDesc,
                       GENERICAPP_CLUSTERID,
                       Leng,
                       buf,
                       &GenericApp_TransID,
                       AF_DISCV_ROUTE,
                     //  AF_ACK_REQUEST,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
        {
                return 1;
        }
        else
        {
                return 0;// Error occurred in request to send.
        }
}

void Delay(unsigned n)  
{
 unsigned tt;
 for(tt=0;tt<n;tt++);
 for(tt=0;tt<n;tt++);
 for(tt=0;tt<n;tt++);
 for(tt=0;tt<n;tt++);
 for(tt=0;tt<n;tt++);
}

void UART_Send_String( unsigned char *Data,int len)
{

  HalUARTWrite( SER_PORT,  Data, len);
}


void uprint( uint8 * Data)
{
  UART_Send_String( Data, strlen(Data) );
}


void parse_procotol(uint8*buff, uint8 len)
{
   
  uint8 logicalType;
  char pbuf[3];
  char pbuf1[3];
  char pbuf2[65];
  
  uint16 tmp_ios;
  nwkActiveKeyItems keyItems;
  
  if(len == 1)
  {
        if(buff[0] == 0x01)
        {
          SendData( 0xffff, buff, 1); 
          return;
        } 
        if(buff[0]== 0x02)
        {
          /// short address
          UART_Send_String( (uint8*)&_NIB.nwkDevAddress, 2);
          
        }
        if(buff[0] == 0x05)
        {
          /// pan id
          pbuf[0] = 0x06;
          pbuf[1] = LO_UINT16(_NIB.nwkPanId);
          pbuf[2] = HI_UINT16(_NIB.nwkPanId);
          
          UART_Send_String( (uint8*)&pbuf, 3);          
        }
        if(buff[0] == 0x07)/// channel
        {
            pbuf[0] = 0x08;
            pbuf[1] = _NIB.nwkLogicalChannel;
            UART_Send_String( (uint8*)&pbuf, 2);
        }
        if(buff[0] ==0x03) // mac address
        {
            ieeeAddr = NLME_GetExtAddr();
            
            pbuf2[0] = 0x04;
            strncpy(&pbuf2[1], ieeeAddr,8);
            
            UART_Send_String( (uint8*)&pbuf2, 9);
            
        }
        if( buff[0] ==0xc0) // coordinator 
        {
          logicalType = (uint8)ZDO_Config_Node_Descriptor.LogicalType;
          osal_nv_item_init( ZCD_NV_LOGICAL_TYPE, sizeof(logicalType), &logicalType );
          logicalType = ZG_DEVICETYPE_COORDINATOR;
          if( osal_nv_write( ZCD_NV_LOGICAL_TYPE, 0 ,sizeof(logicalType), &logicalType) != ZSUCCESS)
          {
            uprint("set device to coordi failed");
          }else
          {
            zgWriteStartupOptions (ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE);
            uprint("set device to coordi,restart it");
          }         
          return;
        }

        if( buff[0] ==0xe0) // router
        {
          logicalType = (uint8)ZDO_Config_Node_Descriptor.LogicalType;
          osal_nv_item_init( ZCD_NV_LOGICAL_TYPE, sizeof(logicalType), &logicalType );
          logicalType = ZG_DEVICETYPE_ROUTER;
          if( osal_nv_write( ZCD_NV_LOGICAL_TYPE, 0 ,sizeof(logicalType), &logicalType) != ZSUCCESS)
          {
            uprint("set device to router failed");
          }else
          {
            zgWriteStartupOptions (ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE);
            uprint("set device to router,restart it");
          }
          
          return;
        }

        if( buff[0] ==0xe1) // end device
        {
          logicalType = (uint8)ZDO_Config_Node_Descriptor.LogicalType;
          osal_nv_item_init( ZCD_NV_LOGICAL_TYPE, sizeof(logicalType), &logicalType );
          logicalType = ZG_DEVICETYPE_ENDDEVICE;
          if( osal_nv_write( ZCD_NV_LOGICAL_TYPE, 0 ,sizeof(logicalType), &logicalType) != ZSUCCESS)
          {
            uprint("set device to end device failed");
          }else
          {
            zgWriteStartupOptions (ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE);
            uprint("set device to end device,restart it");
          }
          return;
        }        
  }// len == 1

      if( len == 2)
      {
        if(buff[0] == 0x09)
        {
          if(buff[1] >= 0 && buff[1] <= 7)
          {
             tmp_ios = 0x0202 + buff[1];
             if( osal_nv_item_init( tmp_ios, sizeof(uint8), NULL) == ZSUCCESS)
             {
               osal_nv_read(tmp_ios,0, sizeof(uint8),&pbuf[1]);
               pbuf[0] = 0x09;
               UART_Send_String( (uint8*)&pbuf, 2);
             }else
             {
               uprint("osal_nv_item_init 0x09 failed");
             }
          }
        }
        if(buff[0] == 0x10)
        {
          if(buff[1] >= 0 && buff[1] <= 7)
          {
             tmp_ios = 0x0210 + buff[1];
             if( osal_nv_item_init( tmp_ios, sizeof(uint8)*64, NULL) == ZSUCCESS)
             {
               osal_nv_read(tmp_ios,0, sizeof(uint8)*64,&pbuf2[1]);
               pbuf[0] = 0x10;
               UART_Send_String( (uint8*)&pbuf2, 65);
             }else
             {
               uprint("osal_nv_item_init 0x10 failed");
             }
          }          
        }
        
        if( buff[0] == 0xcc )
        {
          if( buff[1] > 0x0a && buff[1] < 0x1b )
          {

            _NIB.nwkLogicalChannel = buff[1];
            NLME_UpdateNV(0x01);
            ZMacSetReq( ZMacChannel, &buff[1]);
            
            osal_nv_item_init( ZCD_NV_CHANLIST, sizeof(zgDefaultChannelList), &zgDefaultChannelList);
            
            if( buff[1] == 0x0b)
            {
              zgDefaultChannelList = 0x00000800;
              
            } 
            if (buff[1] == 0x0c )
            {
              zgDefaultChannelList = 0x00001000;
            }            
            if (buff[1] == 0x0d )
            {
              zgDefaultChannelList = 0x00002000;
            }
            if (buff[1] == 0x0e )
            {
              zgDefaultChannelList = 0x00004000;
            }
            if (buff[1] == 0x0f )
            {
              zgDefaultChannelList = 0x00008000;
            }
            if (buff[1] == 0x10 )
            {
              zgDefaultChannelList = 0x00010000;
            }
            if (buff[1] == 0x11 )
            {
              zgDefaultChannelList = 0x00020000;
            }
            if (buff[1] == 0x12 )
            {
              zgDefaultChannelList = 0x00040000;
            }            
            if (buff[1] == 0x13 )
            {
              zgDefaultChannelList = 0x00080000;
            }
            if (buff[1] == 0x14 )
            {
              zgDefaultChannelList = 0x00100000;
            }
            if (buff[1] == 0x15 )
            {
              zgDefaultChannelList = 0x00200000;
            }            
            if (buff[1] == 0x16 )
            {
              zgDefaultChannelList = 0x00400000;
            }
            if (buff[1] == 0x17 )
            {
              zgDefaultChannelList = 0x00800000;
            }
            if (buff[1] == 0x18 )
            {
              zgDefaultChannelList = 0x01000000;
            }

            if (buff[1] == 0x19 )
            {
              zgDefaultChannelList = 0x02000000;
            }
            if (buff[1] == 0x1a )
            {
              zgDefaultChannelList = 0x04000000;
            }
            
            osal_nv_write(  ZCD_NV_CHANLIST, 0 ,sizeof(zgDefaultChannelList), &zgDefaultChannelList);
            
            UART_Send_String( (uint8*)&zgDefaultChannelList, sizeof(zgDefaultChannelList) );

            /*
            _NIB.nwkLogicalChannel = buff[1];
            ZMacSetReq( ZMacChannel, &buff[1]);
            ZDApp_NwkStateUpdateCB();
            _NIB.nwkTotalTransmissions = 0;
            nwkTransmissionFailures( TRUE );
            */
          }
        }
      }  
  
      if( len == 3)
      {
#if defined( ZDO_COORDINATOR )
    // only coordinator can have this function
      
      if( buff[0] == 0xCA || buff[0] == 0x03 || buff[0] == 0x01 )
      {
        // CA xx xx ,send CA to a node ,with it's short address, 0x03 ,mac address, 0x01 short address
        ///uprint("it's CA ");
        short_ddr = BUILD_UINT16( buff[1], buff[2] );        
        if(  SendData( short_ddr, buff, 1) == 0 )
        {
          uprint("send failed,0xca 0x03 0x01");
        }
        return;
      }
    
#endif 
        if(buff[0] == 0xed)
        {
          if(buff[1] == 0xfa)
          {
            if(buff[2] == 0x01)
            {
              LED1 = 1;return;
            }
            if(buff[2] == 0x02)
            {
              LED2 = 1;return;
            }
          }
          if(buff[1] ==0xff)
          {
            if(buff[2] == 0x01)
            {
              LED1 = 0;return;
            }
            if(buff[2] == 0x02)
            {
              LED2 = 0;return;
            }
          }
          if(buff[1] == 0xfc)
          {
            if(buff[2]== 0x01)
            {
              LED1= !LED1;return;
            }
            if(buff[2] == 0x02)
            {
              LED2 = !LED2;return;
            }
          }
          
        }
        
        if(buff[0] == 0xdc)
        {
          if( buff[1] >= 0 && buff[1] <= 0x0f)
          {
            switch (buff[2])
            {
            case 8:
              UART_Send_String( (uint8*)HalAdcRead ( buff[1], HAL_ADC_RESOLUTION_8), 2);
              break;
            case 10:
              UART_Send_String( (uint8*)HalAdcRead ( buff[1], HAL_ADC_RESOLUTION_10), 2);
              break;
            case 12:
              UART_Send_String( (uint8*)HalAdcRead ( buff[1], HAL_ADC_RESOLUTION_12), 2);
              break;
            case 14:
              UART_Send_String( (uint8*)HalAdcRead ( buff[1], HAL_ADC_RESOLUTION_14), 2);
              break;
            default:
              UART_Send_String( (uint8*)HalAdcRead ( buff[1], 0), 2);
              break;
            }
          }
        }
        
      }
      
      if( len == 6)
      {
        if( strstr( buff,"reboot") )
        {
          SystemReset();
        }
        
        if(buff[0] == 'p' && buff[1]==':') // pan id
        {
           // NLME_SetDefaultNV();
            strncpy(pbuf, &buff[2],2); pbuf[2] = '\0';
            strncpy(pbuf1, &buff[4],2); pbuf1[2] = '\0';
            
            set_panid( BUILD_UINT16( strtol(pbuf1,NULL,16),strtol(pbuf,NULL,16) ));
            
            //_NIB.nwkPanId = BUILD_UINT16( strtol(pbuf1,NULL,16),strtol(pbuf,NULL,16));

            
//            NLME_UpdateNV(0x01);
            
            /*
            ZAddr.addr.shortAddr = NLME_GetShortAddr();
            ZAddr.addrMode = Addr16Bit;
            ZDP_MgmtLeaveReq( &ZAddr, NLME_GetExtAddr(), 0 ); 
            */
            
            
            if(_NIB.nwkPanId == 0xffff)
            {
              zgWriteStartupOptions (ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE);
              
              /*
              if( _NIB.nwkLogicalChannel == 0x0b )
              {
                zgDefaultChannelList = 0x00000800;  
              } 
              if ( _NIB.nwkLogicalChannel == 0x0c )
              {
                zgDefaultChannelList = 0x00001000;
              }
              */
              /*
              osal_nv_item_init( ZCD_NV_CHANLIST, sizeof(zgDefaultChannelList), &zgDefaultChannelList);
              osal_nv_read(  ZCD_NV_CHANLIST, 0, sizeof(zgDefaultChannelList), &zgDefaultChannelList);
              
              UART_Send_String( (uint8*)&zgDefaultChannelList, sizeof(zgDefaultChannelList) );
              */
              //ZDOInitDevice( 0 );
              
              /*
              if ( ZG_BUILD_COORDINATOR_TYPE && logicalType == NODETYPE_COORDINATOR )
              {
               // if( 
                NLME_NetworkFormationRequest( zgConfigPANID, zgApsUseExtendedPANID, _NIB.nwkLogicalChannel,
                                               zgDefaultStartingScanDuration, BEACON_ORDER_NO_BEACONS , BEACON_ORDER_NO_BEACONS,false);
                //NLME_StartRouterRequest( BEACON_ORDER_NO_BEACONS, BEACON_ORDER_NO_BEACONS, false );
              }
              if ( ZG_BUILD_JOINING_TYPE && (logicalType == NODETYPE_ROUTER || logicalType == NODETYPE_DEVICE) )
              {
                NLME_NetworkDiscoveryRequest( _NIB.nwkLogicalChannel, zgDefaultStartingScanDuration );
                uprint("researching");
              }
              */
              // after reset, search and join new network
            
              SystemReset();
            }
            
        }
        
        if(buff[0] == 's' && buff[1]==':') // short address
        {
            strncpy(pbuf, &buff[2],2); pbuf[2] = '\0';
            strncpy(pbuf1, &buff[4],2); pbuf1[2] = '\0';
            _NIB.nwkDevAddress = BUILD_UINT16( strtol(pbuf1,NULL,16),strtol(pbuf,NULL,16));
            NLME_UpdateNV(0x01);
            //SystemResetSoft();
            
        }
        
        if( strstr(buff, "nwkkey")  )
        {
          if ( osal_nv_read( ZCD_NV_NWKKEY, 0, sizeof(nwkActiveKeyItems), (void*)&keyItems ) ==   ZSUCCESS )
          {
            UART_Send_String( keyItems.active.key, SEC_KEY_LEN);
          }else
          {
            uprint("osal nv read nwkkey error");
          }
        }
      }
  
      if( len == 4 )
      {
        if( strstr( buff,"type") )
        {
          switch(zgDeviceLogicalType)
          {
          case ZG_DEVICETYPE_COORDINATOR:
            uprint("Coordi");
            break;
          case ZG_DEVICETYPE_ROUTER:
            uprint("Router");
            break;
          case ZG_DEVICETYPE_ENDDEVICE:
            uprint("Enddev");
            break;
          default:
            uprint("Unknow");
            break;

          } 
        }
        
        if(buff[0] == 'p' && buff[1] == 'i' && buff[2]=='n' && buff[3] == 'g')
        {
            uprint("pong");
        }
      
      }
  
    if( len == 21 )
    {
      
      if(strstr(buff, "pass:"))
      {
#if defined (SECURE)
          SSP_UpdateNwkKey( (byte*)(buff+5), 0);
          SSP_SwitchNwkKey( 0 );
	  ZDApp_NVUpdate();
          uprint("change key");
#endif
      }
    }
    
    if( len > 3 )
    {
      if(buff[0] == 0x09)
      {
        if(buff[1] >= 0 && buff[1] <= 7)
        {
             tmp_ios = 0x0202 + buff[1];
             if( osal_nv_item_init( tmp_ios, sizeof(uint8), NULL) == SUCCESS)
             {
               osal_nv_write(tmp_ios,0, sizeof(uint8),&buff[2]);
             }else
             {
               uprint("osal_nv_item_init 0x09 failed");
             }
        }
      }
      if(buff[0] == 0x10)
      {
        if(buff[1] >= 0 && buff[1] <= 7)
        {
          tmp_ios = 0x0210 + buff[1];
          if( osal_nv_item_init( tmp_ios, 64*sizeof(uint8), NULL) == SUCCESS)
          {
            osal_nv_write(tmp_ios,0, sizeof(uint8)*buff[2],&buff[3]);
          }else
          {
            uprint("osal_nv_item_init 0x10 failed");
          }
        }
      }
    }
}

static void Serial_callBack(uint8 port, uint8 event)
{
  
  zAddrType_t dstAddr;

  
  uint8 buff[32];
  uint8 readBytes = 0;
  

  
  uint8 yy1;

  
  uint8 i;
  byte nr;  
  

  

  //short_ddr = GenericApp_DstAddr.addr.shortAddr;

//     uint16 devlist[ NWK_MAX_DEVICES + 1];
    
  
  readBytes = HalUARTRead(SER_PORT, buff, 31);
  if (readBytes > 0)
  {
    //HalUARTWrite( SER_PORT, "DataRead: ",10);
    // HalUARTWrite( SER_PORT, buff, readBytes);
    
    if( readBytes == 1)
    {   
        if(buff[0] == 0xCA)
        {
          // 使命的招唤
          // read self AssociatedDevList
          yy1 = 1;
          for(i=0;i< NWK_MAX_DEVICES; i++)
          {
             nr = AssociatedDevList[ i ].nodeRelation;
             //if(nr > 0 && nr < 5) //CHILD_RFD CHILD_RFD_RX_IDLE CHILD_FFD CHILD_FFD_RX_IDLE
             if( nr != 0XFF)
             {
               //   myaddr.addrMode = (afAddrMode_t)Addr16Bit;
               //   myaddr.endPoint = GENERICAPP_ENDPOINT;
             //  if( AssociatedDevList[ i ].shortAddr != 0x0000)
             //  {
                  //if( AssocIsChild( AssociatedDevList[ i ].shortAddr ) != 1 || AssociatedDevList[ i ].age > NWK_ROUTE_AGE_LIMIT)
                  //if( AssocIsChild( AssociatedDevList[ i ].shortAddr ) == 1 && AssociatedDevList[ i ].age > NWK_ROUTE_AGE_LIMIT )
                 // {
                    //  myaddr.addr.shortAddr = AssociatedDevList[ i ].shortAddr;
                      /*
                      if ( AF_DataRequest( &myaddr, &GenericApp_epDesc, GENERICAPP_CLUSTERID,(byte)osal_strlen( theMessageData ) + 1,
                              (byte *)&theMessageData,
                              &GenericApp_TransID,
                              AF_ACK_REQUEST, AF_DEFAULT_RADIUS ) != afStatus_SUCCESS )
                        {
                          uprint("delete asso");
                        */
                   //       delete_asso( AssociatedDevList[ i ]. addrIdx);
                          
                  // }
                    //    else
                     //   {
                        //  if( ZDSecMgrAuthenticationCheck(AssociatedDevList[ i ].shortAddr) == TRUE)
                          {
                            devlist[yy1] = AssociatedDevList[ i ].shortAddr;
                            yy1++;
                          }
                     //   }
                  
                  // }
             }//else {break;}
          }
          devlist[0] = BUILD_UINT16(0xce,  AssocCount( PARENT, CHILD_FFD_RX_IDLE ) );
          ///devlist[0] = BUILD_UINT16(0xce,  yy1-1 );
          UART_Send_String( (uint8*)&devlist[0], yy1*2);
            //p1 = AssocMakeList( &cnt );
            //UART_Send_String( (uint8*)p1,  AssocCount(1, 4)*2);
            //osal_mem_free(p1);
          return;
        }
    }
    
    
    
    if( readBytes >= 2)
    {
      if( buff[0] == 'e' && buff[1] == '#')
      {
          uprint("EndDevice match");
          HalLedSet ( HAL_LED_1, HAL_LED_MODE_OFF );

          dstAddr.addrMode = Addr16Bit;
          dstAddr.addr.shortAddr = 0x0000; // Coordinator
          ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(), GenericApp_epDesc.endPoint,
                            GENERICAPP_PROFID,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            FALSE );
      
      }
 
      if( buff[0] == 'r' && buff[1] == '#')
      {
          uprint("Router Device match");
          
//        HalLedSet ( HAL_LED_1, HAL_LED_MODE_FLASH );

      dstAddr.addrMode = AddrBroadcast;
      dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
      ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                        GENERICAPP_PROFID,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        FALSE );
      
      }


     
      
      if(buff[0] == 0xd8 ) // d8 xx xx i o t ,etc,d8 xx xx ed f[a,c,f] 01
      {
          //short_ddr = BUILD_UINT16(buff[1],buff[2]);
         // if(short_ddr != 0x0000)
          //{
            SendData( BUILD_UINT16(buff[1],buff[2]), &buff[3], readBytes - 3);
          //}else
          //{
            // send to myself
          //}
            return;
      }
      
    }


  }  
}

void serial_init(void )
{
  halUARTCfg_t uartConfig;

 
  uartConfig.configured           = TRUE;                                                   // 2x30 don't care - see uart driver.
  uartConfig.baudRate             = HAL_UART_BR_38400;                       //38400
  uartConfig.flowControl          = FALSE;
  uartConfig.flowControlThreshold = 64;        // 64.
  uartConfig.rx.maxBufSize        = 512;                  // 128
  uartConfig.tx.maxBufSize        = 512;                  // 128
  uartConfig.idleTimeout          = 6;                      // 6
  uartConfig.intEnable            = TRUE;                                                 
  uartConfig.callBackFunc         = Serial_callBack;                   // Call back function
  
  HalUARTOpen (SER_PORT, &uartConfig);

}

static void init_timer3_as_second_counter(void)
{
    T3CTL &= ~0x10;             // Stop timer 3 (if it was running)
    T3CTL |= 0x04;              // Clear timer 3
    T3CTL |= 0x08;             // enable Timer 3 overflow interrupts
    T3CTL |= 0x00;              // Timer 3 mode = free-running mode,with overflowe interrupt
    T3CTL |= 224; //  Tick frequency/128
    T3IE = 1;  
    
}

void io_init(void)
{
    /* For falling edge, the bit must be set. */
/*
    PICTL &= ~(  HAL_KEY_JOY_MOVE_BIT ); 
    PICTL |= HAL_KEY_JOY_MOVE_BIT;

    P0IEN  |= HAL_KEY_SW_6_BIT;
    IEN1 |= BV(5);
    P0IFG = ~(HAL_KEY_SW_6_BIT);
*/
 // P0DIR &= ~0x01;
  
 // P0DIR |= 0x80;
  
 // P0IFG &= ~0x01;
  P1DIR |= 0x03; 
 // P0_7 = 1;
  P1_1 = 0;
  P1_0 = 0;
  
   
}

void set_panid( uint16 u16NewPanid)
{
  uint8 u8BackCode;
  _NIB.nwkPanId = u16NewPanid;
  macRadioSetPanID ( _NIB.nwkPanId);
  ZMacSetReq( ZMacPanId, (byte *)& _NIB.nwkPanId);
  
  u8BackCode = osal_nv_write(ZCD_NV_PANID, 0, 2, &u16NewPanid);
  if( u8BackCode == ZSUCCESS)
  {
    NLME_UpdateNV(0x01);
//    HAL_SYSTEM_RESET();
  }
  else
  {
    uprint("set_panid failed");
  }
  
}

uint8 cb_ReadConfiguration( uint8 configId, uint8 len, void *pValue ) // it's just check the configId if existed,wont create one,unlikely osal_nv_item_init
{
  uint8 size;

  size = (uint8)osal_nv_item_len( configId );
  if ( size > len )
  {
    return ZFailure;
  }
  else
  {
    return( osal_nv_read(configId, 0, size, pValue) );
  }
}

uint8 cb_WriteConfiguration( uint8 configId, uint8 len, void *pValue )
{
  return( osal_nv_write(configId, 0, len, pValue) );
}


/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
static void GenericApp_HandleKeys( byte shift, byte keys );
static void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
static void GenericApp_SendTheMessage( afIncomingMSGPacket_t* );

#if defined( IAR_ARMCM3_LM )
static void GenericApp_ProcessRtosMessage( void );
#endif

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */


void GenericApp_Init( uint8 task_id )
{
  
//  zclSE_Init( &GenericApp_SimpleDesc );
  uint16 size;
  
  
  GenericApp_TaskID = task_id;
  GenericApp_NwkState = DEV_INIT;
  GenericApp_TransID = 0;

  // Device hardware initialization can be added here or in main() (Zmain.c).
  // If the hardware is application specific - add it here.
  // If the hardware is other parts of the device add it in main().

  GenericApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  GenericApp_DstAddr.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_DstAddr.addr.shortAddr = 0xFFFF;

  // Fill out the endpoint description.
  GenericApp_epDesc.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_epDesc.task_id = &GenericApp_TaskID;
  GenericApp_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&GenericApp_SimpleDesc;
  GenericApp_epDesc.latencyReq = noLatencyReqs;

  // Register the endpoint description with the AF
  afRegister( &GenericApp_epDesc );


  RegisterForKeys( GenericApp_TaskID );

  // Update the display
#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "GenericApp", HAL_LCD_LINE_1 );
#endif

#if !defined( ZDO_COORDINATOR )
  ZDO_RegisterForZDOMsg( GenericApp_TaskID, End_Device_Bind_rsp );
  ZDO_RegisterForZDOMsg( GenericApp_TaskID, Match_Desc_rsp );
  
#endif


#if defined( IAR_ARMCM3_LM )

  RTOS_RegisterApp( task_id, GENERICAPP_RTOS_MSG_EVT );
#endif
  
#if defined ( BUILD_ALL_DEVICES )
  size = osal_nv_item_len( ZCD_NV_LOGICAL_TYPE );
  if( size  <= sizeof(zgDeviceLogicalType) && size != 0 )
  {
    cb_ReadConfiguration( ZCD_NV_LOGICAL_TYPE, sizeof(zgDeviceLogicalType), &zgDeviceLogicalType);
  }
  
#endif

#if defined ( HOLD_AUTO_START )
    // HOLD_AUTO_START is a compile option that will surpress ZDApp
    //  from starting the device and wait for the application to
    //  start the device.
  
  if(zgDeviceLogicalType == ZG_DEVICETYPE_COORDINATOR)
    uprint("Coord0\n");
  if(zgDeviceLogicalType == ZG_DEVICETYPE_ROUTER)
    uprint("Router0\n");
  
  ZDOInitDevice(0);
  
  if(zgDeviceLogicalType == ZG_DEVICETYPE_COORDINATOR)
    uprint("Coord\n");
  if(zgDeviceLogicalType == ZG_DEVICETYPE_ROUTER)
    uprint("Router\n");

  
#endif   
  
  io_init();
  
  serial_init( );

  
//  HalLedSet( HAL_LED_2, HAL_LED_MODE_ON );
//  HalLedSet( HAL_LED_1, HAL_LED_MODE_ON );
//  NLME_PermitJoiningRequest(0xFE);

  init_timer3_as_second_counter();
  
}

uint16 GenericApp_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  afDataConfirm_t *afDataConfirm;

  // Data Confirmation message fields
  byte sentEP;
  ZStatus_t sentStatus;
  byte sentTransID;       // This should match the value sent
  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZDO_CB_MSG:
          GenericApp_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          {
           // HalLedSet ( HAL_LED_1, HAL_LED_MODE_BLINK );
           // UART_Send_String("KEY_CHANGE", 10);
            GenericApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          }
          break;

        case AF_DATA_CONFIRM_CMD:
          // This message is received as a confirmation of a data packet sent.
          // The status is of ZStatus_t type [defined in ZComDef.h]
          // The message fields are defined in AF.h
          afDataConfirm = (afDataConfirm_t *)MSGpkt;
          sentEP = afDataConfirm->endpoint;
          sentStatus = afDataConfirm->hdr.status;
          sentTransID = afDataConfirm->transID;
          (void)sentEP;
          (void)sentTransID;

          // Action taken when confirmation is received.
          if ( sentStatus != ZSuccess )
          {
            // The data wasn't delivered -- Do something
            uprint("AF_DATA_CONFIRM_CMD ERR");
            
          }
          break;
                  
        case AF_INCOMING_MSG_CMD:
          
          
          GenericApp_MessageMSGCB( MSGpkt );
          break;

        case ZDO_STATE_CHANGE:
          GenericApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if ( (GenericApp_NwkState == DEV_ZB_COORD)
              || (GenericApp_NwkState == DEV_ROUTER)
              || (GenericApp_NwkState == DEV_END_DEVICE) )
          {
/*
            osal_start_timerEx( GenericApp_TaskID,
                                GENERICAPP_SEND_MSG_EVT,
                                GENERICAPP_SEND_MSG_TIMEOUT );
*/
          }
          global_start = 1;
          break;

        default:
          break;
      }

      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );

      // Next
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  // Send a message out - This event is generated by a timer
  //  (setup in GenericApp_Init()).


  
#if defined( IAR_ARMCM3_LM )
  // Receive a message from the RTOS queue
  if ( events & GENERICAPP_RTOS_MSG_EVT )
  {
    // Process message from RTOS queue
    GenericApp_ProcessRtosMessage();

    // return unprocessed events
    return (events ^ GENERICAPP_RTOS_MSG_EVT);
  }
#endif

  // Discard unknown events
  return 0;
}


static void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg )
{
  
  
  switch ( inMsg->clusterID )
  {
    case End_Device_Bind_rsp:
      uprint("End_Device_Bind_rsp");
      if ( ZDO_ParseBindRsp( inMsg ) == ZSuccess )
      {
        // Light LED
        HalLedSet( HAL_LED_2, HAL_LED_MODE_ON );
      }
#if defined( BLINK_LEDS )
      else
      {
        // Flash LED to show failure
        //HalLedSet ( HAL_LED_1, HAL_LED_MODE_FLASH );
      }
#endif
      break;

    case Match_Desc_rsp:
      {
        uprint("Match_Desc_rsp");
        
        ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( inMsg );
        if ( pRsp )
        {
          if ( pRsp->status == ZSuccess && pRsp->cnt )
          {
            GenericApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
            GenericApp_DstAddr.addr.shortAddr = pRsp->nwkAddr;
            // Take the first endpoint, Can be changed to search through endpoints
            GenericApp_DstAddr.endPoint = pRsp->epList[0];

            // Light LED
            //HalLedSet( HAL_LED_1, HAL_LED_MODE_ON );
          }
          osal_mem_free( pRsp );
        }
      }
      break;
  default:
      uprint("GenericApp_ProcessZDOMsgs");
      break;
  }
}

/*********************************************************************
 * @fn      GenericApp_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_4
 *                 HAL_KEY_SW_3
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
static void GenericApp_HandleKeys( uint8 shift, uint8 keys )
{


  // Shift is used to make each button/switch dual purpose.
  ///UART_Send_String( (byte*)keys, 1);

    if ( keys & HAL_KEY_SW_1 )
    {
       uprint("key1");
       if(global_start == 1)
       {
          T3CTL |= 0x10;
       }
    }
    if ( keys & HAL_KEY_SW_2 )
    {
      uprint("key2");
    }
    if ( keys & HAL_KEY_SW_3 )
    {
      uprint("key3");
    }
    if ( keys & HAL_KEY_SW_4 )
    {
      uprint("key4");
    }
    
    if ( keys & HAL_KEY_SW_5 )
    {
      uprint("key5");
    }

    /*
    if ( keys & HAL_KEY_SW_6 )
    {
      uprint("key6");
    }
    */
    if ( keys & HAL_KEY_SW_7 )
    {
      uprint("key7");
    }
   
/*  
  if ( shift )
  {
 
    if ( keys & HAL_KEY_SW_1 )
    {
       uprint("key1");
    }
    if ( keys & HAL_KEY_SW_2 )
    {
      uprint("key2");
    }
    if ( keys & HAL_KEY_SW_3 )
    {
      uprint("key3");
    }
    if ( keys & HAL_KEY_SW_4 )
    {
      uprint("key4");
    }
    
    if ( keys & HAL_KEY_SW_5 )
    {
      uprint("key5");
    }

    if ( keys & HAL_KEY_SW_6 )
   {
      uprint("key6");
   }
   if ( keys & HAL_KEY_SW_7 )
   {
      uprint("key7");
   }
  }
  else
  {

    if ( keys & HAL_KEY_SW_2 )
    {
      uprint("key2 no s");
      HalLedSet ( HAL_LED_1, HAL_LED_MODE_OFF );

      // Initiate an End Device Bind Request for the mandatory endpoint
      dstAddr.addrMode = Addr16Bit;
      dstAddr.addr.shortAddr = 0x0000; // Coordinator
      ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(),
                            GenericApp_epDesc.endPoint,
                            GENERICAPP_PROFID,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            FALSE );
    }

    if ( keys & HAL_KEY_SW_1 )
    {
      uprint("key1 no s");
    }
    
    if ( keys & HAL_KEY_SW_4 )
   {
      uprint("key4 no s");
      HalLedSet ( HAL_LED_1, HAL_LED_MODE_OFF );
      // Initiate a Match Description Request (Service Discovery)
      dstAddr.addrMode = AddrBroadcast;
      dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
      ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                        GENERICAPP_PROFID,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        FALSE );
    }
    
    if ( keys & HAL_KEY_SW_6 )
    {
      uprint("key6 no s");
      AF_DataRequest( &GenericApp_DstAddr, &GenericApp_epDesc,
                       GENERICAPP_CLUSTERID,
                       6,
                       "hello6",
                       &GenericApp_TransID,
                       AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );         
    }   

    if ( keys & HAL_KEY_SW_7 )
    {
      uprint("key7 no s");    
    }
  }
*/
  /*
      HalLedSet ( HAL_LED_1, HAL_LED_MODE_FLASH );

      dstAddr.addrMode = AddrBroadcast;
      dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
      ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                        GENERICAPP_PROFID,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        FALSE );
    */  

}

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      GenericApp_MessageMSGCB
 *
 * @brief   Data message processor callback.  This function processes
 *          any incoming data - probably from other devices.  So, based
 *          on cluster ID, perform the intended action.
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
  unsigned char buff[12];

  byte nr;
  uint8 i;
  uint8 ass_num;
  uint8*u8p;
  
  uint8 yy1;
  
  
  afAddrType_t myaddr; // use for p2p
  
  ass_num = 0;
  
  switch ( pkt->clusterId )
  {
    case GENERICAPP_CLUSTERID:
    if( pkt->cmd.DataLength == 1)
    {      
        yy1 = 1;
        if(pkt->cmd.Data[0] == 0x01) /// GET short address 
        {
          buff[0] = 0x02;
          buff[1] = LO_UINT16(_NIB.nwkDevAddress);
          buff[2] = HI_UINT16(_NIB.nwkDevAddress);
          SendData( pkt->srcAddr.addr.shortAddr, buff, 3);
        }
#if !defined( ZDO_COORDINATOR ) 
        if(pkt->cmd.Data[0] == 0x03)
        {
          /// return mac address : ac xx xx ieee addr
               myaddr.addrMode = (afAddrMode_t)Addr16Bit;
               myaddr.endPoint = GENERICAPP_ENDPOINT;          
               myaddr.addr.shortAddr = 0x0000;
               
            ieeeAddr = NLME_GetExtAddr();
            osal_cpyExtAddr(buff+3, ieeeAddr);
            buff[0] = 0x04;
            buff[1] = LO_UINT16(_NIB.nwkDevAddress);
            buff[2] = HI_UINT16(_NIB.nwkDevAddress);
            AF_DataRequest( &myaddr, &GenericApp_epDesc, GENERICAPP_CLUSTERID,  11,
                          &buff[0],
                          &GenericApp_TransID,
                          AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
                 
            
        }
        // only routers return devlist
        if( pkt->cmd.Data[0] == 0xCA )
        {
               myaddr.addrMode = (afAddrMode_t)Addr16Bit;
               myaddr.endPoint = GENERICAPP_ENDPOINT;          
               myaddr.addr.shortAddr = 0x0000;
          
        ass_num = AssocCount(0, 4);
      //  if( ass_num > 0)
        //{
          for(i=0;i< NWK_MAX_DEVICES; i++)
          {
             nr = AssociatedDevList[ i ].nodeRelation;
              if(nr > 0 && nr < 5) //CHILD_RFD CHILD_RFD_RX_IDLE CHILD_FFD CHILD_FFD_RX_IDLE
             //if( nr != 0XFF)
             {
             //  if( AssociatedDevList[ i ].shortAddr != 0x0000)
             //  {
                  //if( AssocIsChild( AssociatedDevList[ i ].shortAddr ) != 1 || AssociatedDevList[ i ].age > NWK_ROUTE_AGE_LIMIT)
                  //if( AssocIsChild( AssociatedDevList[ i ].shortAddr ) == 1 && AssociatedDevList[ i ].age > NWK_ROUTE_AGE_LIMIT )
                 // {
                    //  myaddr.addr.shortAddr = AssociatedDevList[ i ].shortAddr;
                      /*
                      if ( AF_DataRequest( &myaddr, &GenericApp_epDesc, GENERICAPP_CLUSTERID,(byte)osal_strlen( theMessageData ) + 1,
                              (byte *)&theMessageData,
                              &GenericApp_TransID,
                              AF_ACK_REQUEST, AF_DEFAULT_RADIUS ) != afStatus_SUCCESS )
                        {
                          uprint("delete asso");
                        */
                   //       delete_asso( AssociatedDevList[ i ]. addrIdx);
                          
                  // }
                    //    else
                     //   {
                          devlist[yy1] = AssociatedDevList[ i ].shortAddr;
                          yy1++;
                     //   }
                  
                  // }
             }
              else { break; }
          }
       // }else
        //{
          //devlist[yy1] = 0;
        //}
          devlist[0] = BUILD_UINT16(0xce,  ass_num );
          if ( AF_DataRequest( &myaddr, &GenericApp_epDesc, GENERICAPP_CLUSTERID, yy1*2,
                          (byte *)&devlist,
                          &GenericApp_TransID,
                          AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) != afStatus_SUCCESS )
          {
              uprint("send failed");
          }
        
          
          //cb_SendDataRequest( 0x0000, GENERICAPP_CLUSTERID, yy1*2+1, (uint8*)&devlist[0], 0, AF_DISCV_ROUTE, 0 );
          
          //UART_Send_String( (uint8*)&devlist[0], yy1*2);
          
            //p1 = AssocMakeList( &cnt );
            //UART_Send_String( (uint8*)p1,  AssocCount(1, 4)*2);
            //osal_mem_free(p1);
        }
#endif  
        
    } //if( pkt->cmd.DataLength == 1)
    
    if( pkt->cmd.DataLength == 3)
    { 
        //ed fa 01 ,turn on led 1
        //ed ff 01  turn off led 1
        if( pkt->cmd.Data[0] == 0xed  && pkt->cmd.Data[1] == 0xfa )
        {
            switch(pkt->cmd.Data[2])
            {
              case 0x01:
                LED1 = 1;
                break;
              case 0x02:
                 LED2 = 1;
                break;
              default:break;
            }
                   
        }
        if( pkt->cmd.Data[0] == 0xed  && pkt->cmd.Data[1] == 0xff )
        {
            switch(pkt->cmd.Data[2])
            {
              case 0x01:
                LED1 = 0;
                break;
              case 0x02:
                 LED2 = 0;
                break;
              default:break;
            }          
        }
        if( pkt->cmd.Data[0] == 0xed  && pkt->cmd.Data[1] == 0xfc ) //只是取反
        {
            switch(pkt->cmd.Data[2])
            {
              case 0x01:
                LED1 = !LED1;
                break;
              case 0x02:
                 LED2 = !LED2;
                break;
              default:break;
            }          
        }
        
    } // if( pkt->cmd.DataLength == 3)
    if( pkt->cmd.DataLength == 4)
    {
      if( pkt->cmd.Data[0] == 'p' && pkt->cmd.Data[1] =='i' && pkt->cmd.Data[2] == 'n' && pkt->cmd.Data[3] == 'g')
      {
          buff[0] = pkt->LinkQuality;
          buff[1] = pkt->SecurityUse;
          u8p = (uint8*)&pkt->timestamp;
          
          buff[2] = u8p[0];
          buff[3] = u8p[1];
          buff[4] = u8p[2];
          buff[5] = u8p[3];
          
          SendData( pkt->srcAddr.addr.shortAddr, buff, 6);
      }
    }
    
#if defined( LCD_SUPPORTED )
      UART_Send_String( &pkt->cmd.Data[0], pkt->cmd.DataLength);
#endif
    
      break;
  default:break;
  }
  
  

  
}

/*********************************************************************
 * @fn      GenericApp_SendTheMessage
 *
 * @brief   Send "the" message.
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_SendTheMessage( afIncomingMSGPacket_t *pkt )
{
  

  
  char theMessageData[] = "Hello World";

  
  if ( AF_DataRequest( &pkt->srcAddr, &GenericApp_epDesc,
                       GENERICAPP_CLUSTERID,
                       (byte)osal_strlen( theMessageData ) + 1,
                       (byte *)&theMessageData,
                       &GenericApp_TransID,
                       AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
    // Successfully requested to be sent.
    uprint("ok");
  }
  else
  {
    uprint("err");
    // Error occurred in request to send.
  }
}

#if defined( IAR_ARMCM3_LM )
/*********************************************************************
 * @fn      GenericApp_ProcessRtosMessage
 *
 * @brief   Receive message from RTOS queue, send response back.
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_ProcessRtosMessage( void )
{
  osalQueue_t inMsg;

  if ( osal_queue_receive( OsalQueue, &inMsg, 0 ) == pdPASS )
  {
    uint8 cmndId = inMsg.cmnd;
    uint32 counter = osal_build_uint32( inMsg.cbuf, 4 );

    switch ( cmndId )
    {
      case CMD_INCR:
        counter += 1;  /* Increment the incoming counter */
                       /* Intentionally fall through next case */

      case CMD_ECHO:
      {
        userQueue_t outMsg;

        outMsg.resp = RSP_CODE | cmndId;  /* Response ID */
        osal_buffer_uint32( outMsg.rbuf, counter );    /* Increment counter */
        osal_queue_send( UserQueue1, &outMsg, 0 );  /* Send back to UserTask */
        break;
      }
      
      default:
        break;  /* Ignore unknown command */    
    }
  }
}
#endif

/*********************************************************************
 */

/*
#pragma vector=URX0_VECTOR
__interrupt static void UART0_ISR(void)
{
  URX0IF=0;//清标志
  temp=U0DBUF;//读取缓存到temp
}
*/



#pragma vector=T3_VECTOR
__interrupt static void T3_IRQ(void)
{
   t3_counter++;
   if(t3_counter > 911) // this is about 1 seconds
   {
     uprint("t3 overflowed");
     t3_counter=0;
   }
}

