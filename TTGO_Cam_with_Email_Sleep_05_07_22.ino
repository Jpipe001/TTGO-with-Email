/*********
  Rui Santos
  This is modifed from the original !!  For TTGO Camera
  Complete instructions at  https://randomnerdtutorials.com/esp32-cam-send-photos-email/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

//  IMPORTANT   Change ~ Pin definitions  ESP32-CAM / TTGO
//  Press and hold "RST" for One Second if TTGO Battery connected.

//  Board Selections:
//***   Board: "ESP32 Dev Module" or "ESP32 Wrover Module" or "ESP32 Wrover Kit"
//***   Upload Speed: "115200"     *** Double Check!!
//***   Flash Mode: "QIO"          *** Double Check!!
//***   Flash Size: "Default 4MB"  *** Double Check!!
//***   PSRAM: "Enabled"           *** Double Check!!
//***   Partition Scheme: "Default 4MB with spiffs" *** Double Check!!
//      COM Port: Depends          *** Double Check!!

// Include Required Libraries
#include <Arduino.h>
#include "esp_camera.h"
#include "ESP32_MailClient.h"      // May need to download this Libary
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
//#include "soc/soc.h"             // Interupt Handler
//#include "soc/rtc_cntl_reg.h"    // Real Time Clock Handler
//#include "driver/rtc_io.h"       // GPIO Handler
#include "U8x8lib.h"     //  for TTGO https://github.com/olikraus/U8g2_Arduino

// NETWORK CREDENTIALS
const char*        ssid = "NETGEAR46";       //"NETWORK NAME";
const char*    password = "icysea351";       //"PASSWORD";

// CONFIGURE MAIL CLIENT
// To send Emails using Gmail on port 465 (SSL), you need to create an app password: https://support.google.com/accounts/answer/185833
#define emailSenderAccount    "jpipe5000@gmail.com"
#define emailSenderPassword   "dbmd uzta sntu soqm"
#define smtpServer            "smtp.gmail.com"
#define smtpServerPort        465
#define emailSubject          "ESP32-CAM Photo Captured"
#define emailRecipient        "jpipe001@gmail.com"

// The Email Sending data object contains config and data to send
SMTPData smtpData;

// CONFIGURE CAMERA PINS
// Pin definitions for CAMERA_MODEL_TTGO_WHITE (Mine)
#define CAMERA_MODEL_TTGO_WHITE_BME280
#define PWDN_GPIO_NUM     26
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     32
#define SIOD_GPIO_NUM     13
#define SIOC_GPIO_NUM     12
#define Y9_GPIO_NUM       39
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       23
#define Y6_GPIO_NUM       18
#define Y5_GPIO_NUM       15
#define Y4_GPIO_NUM       4
#define Y3_GPIO_NUM       14
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    27
#define HREF_GPIO_NUM     25
#define PCLK_GPIO_NUM     19

// Photo File Name to save in SPIFFS
#define PhotoFileName "/photo.jpg"

// CONFIGURE OLED DRIVER
// I2C OLED Display works with SSD1306 driver U8x8lib.h (text) for TTGO
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RST -1
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ OLED_SCL, /* data=*/ OLED_SDA, /* reset=*/ OLED_RST); // Unbuffered, basic graphics, software I2C
String Display_text;
const char* Display;
const char* Reason;           // WAKE UP Reason

String Filename() {
  return String(__FILE__).substring(String(__FILE__).lastIndexOf("\\") + 1);
}


void setup() {
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout detector

  Serial.begin(115200);
  delay(500);
  Serial.println("\nProgram ~ " + Filename());
  Serial.printf("Starting ...\n");

  // Configure OLED Display
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  // Examples of Display Codes
  //u8x8.clearDisplay();                          // Clear the OLED display buffer.
  //u8x8.drawString(0, 0, "STARTING UP");         // Display on OLED  Normal Size
  //u8x8.draw2x2String(1, 1, "RUNNING");          // Display on OLED  Double Size
  //u8x8.setInverseFont(1);                       // Inverse mode 1= ON 0=OFF
  //u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);                          // Flip mode 1= ON 0=OFF

  // CONNECT to WiFi network:
  Serial.print("WiFi Connecting to " + String(ssid) + " ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.printf(" ~ Connected OK\n");

  // Print the Signal Strength:
  long rssi = WiFi.RSSI() + 100;
  Serial.print("Signal Strength = " + String(rssi));
  if (rssi > 50)  Serial.printf(" (>50 - Good)\n");  else   Serial.printf(" (Could be Better)\n");

  // Display on the OLED
  u8x8.clearDisplay();                  // Clear the OLED display
  u8x8.drawString(1, 1, "WiFi CONNECTED");    // On OLED
  Display_text = String(ssid);
  Display = Display_text.c_str();
  u8x8.drawString(1, 2, Display);       // On OLED
  Display_text = "Strength = " + String(rssi);
  Display = Display_text.c_str();
  u8x8.drawString(1, 3, Display);       // On OLED

  Serial.setDebugOutput(true);

  // Set Up SPIFFS
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.printf("An Error has occurred while Mounting SPIFFS\n");
    while (1)
      delay(1000);          // Stay here
  } else {
    Serial.printf(" ~ SPIFFS Mounted Successfully\n");
  }

  // Configure Camera Settings
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Initial setting are FRAMESIZE_UXGA (320x240),  Reset to SVGA (800x600)
  // Initalize with high specs to pre-allocate larger buffers
  if (psramFound()) {
    Serial.printf(" ~ PSRAM Found OK! Frame Set to SVGA\n");
    config.frame_size = FRAMESIZE_SVGA;     //  (800x600)
    config.jpeg_quality = 10; // 10-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    Serial.printf("NO PSRAM found, Check Board Configuration !! Frame Set to UXGA\n");
    config.frame_size = FRAMESIZE_UXGA;    // (320x240)
    config.jpeg_quality = 12;
    config.fb_count = 2;
  }

  // Initialize Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    if (err == 0x105)  Serial.printf("Check Camera Pins Configuration !!\n");
    Serial.printf("Camera init failed with error 0x%x", err);
    u8x8.drawString(1, 5, "Camera Failed");   // On OLED
    while (1)
      delay(1000);           // Stay here
  }

  sensor_t * s = esp_camera_sensor_get();
  // Initial sensors are flipped and colors are a bit saturated
  s->set_vflip(s, 1);       // Flip it Vert
  s->set_hmirror(s, 1);     // Flip it Horz
  s->set_brightness(s, 1);  // Increase the brightness just a bit
  s->set_saturation(s, -2); // Decrease the saturation

  u8x8.drawString(1, 5, "Camera Ready");   // On OLED
  Serial.printf(" ~ Camera Set Up OK!\n");
  Serial.printf("*******************************************\n");

  Capture_Photo_Save_Spiffs();  //  Take Photo, Save to SPIFFS & Email it

  // ********************************************
  // ***  TIMER WAKE UP  or  EXTERNAL WAKE UP  ***
  int Time = 0;   //  ***  in Seconds,  Set to 0 for EXTERNAL WAKE UP  ***
  // ********************************************

  Print_Wakeup_Reason();    //Print the Wakeup Reason

  Serial.printf("Waking Up from %s : Timer Set to: %d Seconds\n", Reason, Time);

  u8x8.clearDisplay();                        // Clear the OLED display
  u8x8.drawString(0, 1, "WAKING UP from:");   // On OLED
  u8x8.drawString(0, 2, Reason);              // On OLED
  u8x8.drawString(0, 3, "GOING TO SLEEP");    // On OLED
  if (Time != 0)  {
    Display_text = "Wait for " + String(Time) + " Secs";
    Display = Display_text.c_str();
    u8x8.drawString(0, 4, Display);           // On OLED
  } else {
    u8x8.drawString(0, 4, "Wait for:");       // On OLED
    u8x8.drawString(0, 5, "New EXT Input");   // On OLED
  }

  // ***  TIMER WAKE UP
  if (Time > 0) esp_sleep_enable_timer_wakeup(Time * 1000000);

  // ***  EXTERNAL WAKE UP
  // PIR = GPIO_NUM_33, 1 = High, 0 = Low Input for TTGO
  if (Time == 0) {
    while (digitalRead(33) == 1)  {  // Wait for GPIO_NUM_33 to Reset
      delay(100);    // Wait a little bit
    }
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 1);
  }
  Serial.printf("Going to Sleep now!\n");
  u8x8.clearDisplay();               // Clear the OLED display buffer.
  Serial.flush();

  esp_deep_sleep_start();
}


void loop() {
  // Nothing to do here!!
}


void Print_Wakeup_Reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  Serial.printf("\n");
  switch (wakeup_reason)  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.printf("Wakeup caused by EXTERNAL Signal using RTC_IO\n");
      Reason = "~ EXT Input";
      break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.printf("Wakeup caused by EXTERNAL Signal using RTC_CNTL\n");
      Reason = "~ EXT Input";
      break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.printf("Wakeup caused by TIMER\n");
      Reason = "~ Timer";
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.printf("Wakeup caused by Touchpad\n"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program\n"); break;
    default : Serial.printf("Wakeup was NOT Caused by Deep Sleep\n");
      Reason = "~ Start Up";
      break;
  }
}


// Capture Photo and Save it to SPIFFS
void Capture_Photo_Save_Spiffs( void ) {
  // Take a photo with the camera ~ Setup frame buffer
  camera_fb_t  * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.printf("Camera capture FAILED\n");
    return;
  }
  // Open file for the photo file
  File file = SPIFFS.open(PhotoFileName, "w+");        // CHANGED to "w+"
  // Check file
  if (!file) {
    Serial.printf("FAILED to open file in writing mode\n");
    return;
  } else {
    // Write data to the photo file
    file.write(fb->buf, fb->len);       // payload (image), payload length
  }
  // Close the file
  Serial.printf("Successfully Saved as \t'%s' - Size: %d bytes\n", PhotoFileName, file.size());
  file.close();

  // Check if file has been correctly Saved in SPIFFS
  File CheckFile = SPIFFS.open(PhotoFileName, "r+");        // CHANGED to "r+"
  Serial.printf("Photo Verified as \t'%s' - Size: %d bytes\n", PhotoFileName, CheckFile.size());
  CheckFile.close();

  // Return the frame buffer back to the driver for reuse
  esp_camera_fb_return(fb);

  Send_Photo();
}


void Send_Photo( void ) {
  // Preparing email
  // Set the SMTP Server Email host, port, account and password
  smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);

  // Set the sender name and Email
  smtpData.setSender("ESP32-CAM", emailSenderAccount);

  // Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
  smtpData.setPriority("High");

  // Set the subject
  smtpData.setSubject(emailSubject);

  // Set the email message in HTML format
  smtpData.setMessage("<h2>Photo from ESP32-CAM attached in this email.</h2>", true);
  // Set the email message in text format
  //smtpData.setMessage("Photo captured with ESP32-CAM and attached in this email.", false);

  // Add recipients, can add more than one recipient
  smtpData.addRecipient(emailRecipient);
  //smtpData.addRecipient(emailRecipient2);

  // Add attach files from SPIFFS
  smtpData.addAttachFile(PhotoFileName, "image/jpg");
  // Set the storage type to attach files in your email (SPIFFS)
  smtpData.setFileStorageType(MailClientStorageType::SPIFFS);

  smtpData.setSendCallback(sendCallback);

  // Start sending Email, can be set callback function to track the status
  if (!MailClient.sendMail(smtpData))  {
    Serial.printf("Error sending Email %s\n", MailClient.smtpErrorReason());
    smtpData.empty();
    return;
  }
  // Clear all data from Email object to free memory
  smtpData.empty();
}

// Callback function to get the Email sending status
void sendCallback(SendStatus msg) {
  //Print the current status
  Serial.println(msg.info());
}
