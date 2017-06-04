#include <ArduinoJson.h>

//********************************************************************************
// Interface for Sending to Controllers
//********************************************************************************
boolean sendData(struct EventStruct *event)
{
  LoadTaskSettings(event->TaskIndex);
  if (Settings.UseRules)
    createRuleEvents(event->TaskIndex);

  if (Settings.GlobalSync && Settings.TaskDeviceGlobalSync[event->TaskIndex])
    SendUDPTaskData(0, event->TaskIndex, event->TaskIndex);

//  if (!Settings.TaskDeviceSendData[event->TaskIndex])
//    return false;

  if (Settings.MessageDelay != 0)
  {
    uint16_t dif = millis() - lastSend;
    if (dif < Settings.MessageDelay)
    {
      uint16_t delayms = Settings.MessageDelay - dif;
      //this is logged nowhere else, so might as well disable it here also:
      // addLog(LOG_LEVEL_DEBUG_MORE, String(F("CTRL : Message delay (ms): "))+delayms);
      delayBackground(delayms);

      // unsigned long timer = millis() + delayms;
      // while (millis() < timer)
      //   backgroundtasks();
    }
  }

  LoadTaskSettings(event->TaskIndex); // could have changed during background tasks.

  for (byte x=0; x < CONTROLLER_MAX; x++)
  {
    event->ControllerIndex = x;
    event->idx = Settings.TaskDeviceID[x][event->TaskIndex];

    if (Settings.TaskDeviceSendData[event->ControllerIndex][event->TaskIndex] && Settings.ControllerEnabled[event->ControllerIndex] && Settings.Protocol[event->ControllerIndex])
    {
      event->ProtocolIndex = getProtocolIndex(Settings.Protocol[event->ControllerIndex]);
      CPlugin_ptr[event->ProtocolIndex](CPLUGIN_PROTOCOL_SEND, event, dummyString);
    }
  }

  PluginCall(PLUGIN_EVENT_OUT, event, dummyString);
  lastSend = millis();
}


/*********************************************************************************************\
 * Handle incoming MQTT messages
\*********************************************************************************************/
// handle MQTT messages
void callback(char* c_topic, byte* b_payload, unsigned int length) {
  // char log[256];
  char c_payload[256];

  statusLED(true);

  if (length>sizeof(c_payload)-1)
  {
    addLog(LOG_LEVEL_ERROR, F("MQTT : Ignored too big message"));
  }

  //convert payload to string, and 0 terminate
  strncpy(c_payload,(char*)b_payload,length);
  c_payload[length] = 0;

  String log;
  log=F("MQTT : Topic: ");
  log+=c_topic;
  addLog(LOG_LEVEL_DEBUG, log);

  log=F("MQTT : Payload: ");
  log+=c_payload;
  addLog(LOG_LEVEL_DEBUG, log);

  // sprintf_P(log, PSTR("%s%s"), "MQTT : Topic: ", c_topic);
  // addLog(LOG_LEVEL_DEBUG, log);
  // sprintf_P(log, PSTR("%s%s"), "MQTT : Payload: ", c_payload);
  // addLog(LOG_LEVEL_DEBUG, log);

  struct EventStruct TempEvent;
  TempEvent.String1 = c_topic;
  TempEvent.String2 = c_payload;
  byte ProtocolIndex = getProtocolIndex(Settings.Protocol[0]);
  CPlugin_ptr[ProtocolIndex](CPLUGIN_PROTOCOL_RECV, &TempEvent, dummyString);
}

/*********************************************************************************************\
 * Connect to MQTT message broker
\*********************************************************************************************/
void MQTTConnect(PubSubClient& _mqttClient, byte ProtIndex)
{
  String log = "";
  ControllerSettingsStruct ControllerSettings;
  LoadControllerSettings(ProtIndex, (byte*)&ControllerSettings, sizeof(ControllerSettings)); // todo index is now fixed to 0 --> ProtIndex

  IPAddress MQTTBrokerIP(ControllerSettings.IP);
  _mqttClient.setServer(MQTTBrokerIP, ControllerSettings.Port);
  //MQTTclient.setServer("guzuir.messaging.internetofthings.ibmcloud.com", 1883);
  _mqttClient.setCallback(callback);

  /*log=F("MQTT : IP : ");
  char str[20];
  sprintf_P(str, PSTR("%u.%u.%u.%u"), ControllerSettings.IP[0], ControllerSettings.IP[1], ControllerSettings.IP[2], ControllerSettings.IP[3]);
  log+= str;
  log+=F(" - ");
  log+=ControllerSettings.Port;
  addLog(LOG_LEVEL_ERROR, log);*/

  // MQTT needs a unique clientname to subscribe to broker

  /**/
  String Device_Id = ControllerSettings.Device_Id;
  String clientid = "";
  if (Device_Id.length() == 0)
  {
    clientid = "ESPClient";
    clientid += Settings.Unit;
  }
  else
  {
    clientid = "d:";
    clientid += ControllerSettings.Org;
    clientid += ":";
    clientid += ControllerSettings.Device_type;
    clientid += ":";
    clientid += ControllerSettings.Device_Id;
  }
  String subscribeTo = "";


  String LWTTopic = ControllerSettings.Subscribe;
  LWTTopic.replace(F("/#"), F("/status"));
  LWTTopic.replace(F("%sysname%"), Settings.Name);

  for (byte x = 1; x < 3; x++)
  {

    boolean MQTTresult = false;

    if ((SecuritySettings.ControllerUser[ProtIndex] != 0) && (SecuritySettings.ControllerPassword[ProtIndex] != 0))
      {
        if (Device_Id.length() == 0)
          MQTTresult = _mqttClient.connect(clientid.c_str(), SecuritySettings.ControllerUser[ProtIndex], SecuritySettings.ControllerPassword[ProtIndex], LWTTopic.c_str(), 0, 0, "Connection Lost");
        else
          {
            MQTTresult = MQTTclient_C012.connect(clientid.c_str(), SecuritySettings.ControllerUser[ProtIndex], SecuritySettings.ControllerPassword[ProtIndex]);
            log=F("MQTT (C012): Connect(1) : ");
            log+=clientid.c_str();
            log+=F(" - ");
            log+=SecuritySettings.ControllerUser[ProtIndex];
            log+=F(" - ");
            log+=SecuritySettings.ControllerPassword[ProtIndex];
            addLog(LOG_LEVEL_ERROR, log);
          }
    }
    else
    {
      MQTTresult = MQTTclient.connect(clientid.c_str(), LWTTopic.c_str(), 0, 0, "Connection Lost");
      /*log=F("MQTT : Connect(2) : ");
      log+=clientid.c_str();
      log+=F(" - ");
      log+=LWTTopic.c_str();
      addLog(LOG_LEVEL_ERROR, log);*/
    }


    if (MQTTresult)
    {
      log = F("MQTT : Connected to broker");

      if (Device_Id.length() == 0)
      {
        addLog(LOG_LEVEL_INFO, log);
        subscribeTo = ControllerSettings.Subscribe;
        subscribeTo.replace(F("%sysname%"), Settings.Name);
        MQTTclient.subscribe(subscribeTo.c_str());
        log = F("Subscribed to: ");
        log += subscribeTo;
        addLog(LOG_LEVEL_INFO, log);

        MQTTclient.publish(LWTTopic.c_str(), "Connected");
      }
      else
      {
        StaticJsonBuffer<300> jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        JsonObject& d = root.createNestedObject("d");
        JsonObject& metadata = d.createNestedObject("metadata");
        metadata["publishInterval"] = 300000;
        JsonObject& supports = d.createNestedObject("supports");
        supports["deviceActions"] = true;

        char buff[300];
        root.printTo(buff, sizeof(buff));
        Serial.println("MQTT (C012): publishing device metadata:"); Serial.println(buff);
        if (MQTTclient_C012.publish(LWTTopic.c_str(), buff)) {
          Serial.println("device Publish ok");
        } else {
          Serial.println("device Publish failed:");
        }
      }

      statusLED(true);
      break; // end loop if succesfull
    }
    else
    {
      log = F("MQTT : Failed to connected to broker");
      addLog(LOG_LEVEL_ERROR, log);
    }

    delay(500);
  }
}


/*********************************************************************************************\
 * Check connection MQTT message broker
\*********************************************************************************************/
void MQTTCheck()
{
// todo index is now fixed to 0
// Aca hay que recorer todos los controladores, verificando si usan MQTT y si estan habilitados par aluego revisar si estan conectados o si necsitan re-coneccion.
  byte ProtocolIndex = getProtocolIndex(Settings.Protocol[0]);
  if ((Protocol[ProtocolIndex].usesMQTT) and (Settings.ControllerEnabled[0]))
    if (!MQTTclient.connected())
    {
      String log = F("MQTT : Connection lost (ProtocolIndex:");
      log += ProtocolIndex;
      log += F(" )");      addLog(LOG_LEVEL_ERROR, log);
      connectionFailures += 2;
      MQTTclient.disconnect();
      delay(1000);
      MQTTConnect(MQTTclient,0);
    }
    else if (connectionFailures)
      connectionFailures--;

// hay que ver si podemos integrar al codigo anterior, y tambien evaluar si podemos hacer que el MQTT Client pase a estar siempre en el controler....
    for (byte x = 0; x < CONTROLLER_MAX; x++)
    {
      if ((getProtocolIndex(Settings.Protocol[x])==10) and (Settings.ControllerEnabled[x]))
      {
        if (!MQTTclient_C012.connected())
        {
          String log = F("MQTT (C012): Connection lost (Protocol Name:");

          String ProtocolName = "";
          CPlugin_ptr[ProtocolIndex](CPLUGIN_GET_DEVICENAME, 0, ProtocolName);
          log += ProtocolName;
          //log += ProtocolIndex;
          log += F(")");
          addLog(LOG_LEVEL_ERROR, log);
          connectionFailures += 2;
          MQTTclient_C012.disconnect();
          delay(1000);
          MQTTConnect(MQTTclient_C012,x);
        }
        else if (connectionFailures)
          connectionFailures--;
        }
      }

}


/*********************************************************************************************\
 * Send status info to request source
\*********************************************************************************************/

void SendStatus(byte source, String status)
{
  switch(source)
  {
    case VALUE_SOURCE_HTTP:
      if (printToWeb)
        printWebString += status;
      break;
    case VALUE_SOURCE_MQTT:
      MQTTStatus(status);
      break;
    case VALUE_SOURCE_SERIAL:
      Serial.println(status);
      break;
  }
}


/*********************************************************************************************\
 * Send status info back to channel where request came from
\*********************************************************************************************/
void MQTTStatus(String& status)
{
  ControllerSettingsStruct ControllerSettings;
  LoadControllerSettings(1, (byte*)&ControllerSettings, sizeof(ControllerSettings)); // todo index is now fixed to 1

  String pubname = ControllerSettings.Subscribe;
  pubname.replace(F("/#"), F("/status"));
  pubname.replace(F("%sysname%"), Settings.Name);
  MQTTclient.publish(pubname.c_str(), status.c_str(),Settings.MQTTRetainFlag);
}
