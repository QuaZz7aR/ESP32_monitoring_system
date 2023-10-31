void configInitCamera(){
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

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     
  s->set_contrast(s, 0);       
  s->set_saturation(s, 0);     
  s->set_special_effect(s, 0); 
  s->set_whitebal(s, 1);       
  s->set_awb_gain(s, 1);       
  s->set_wb_mode(s, 0);        
  s->set_exposure_ctrl(s, 1);  
  s->set_aec2(s, 0);           
  s->set_ae_level(s, 0);       
  s->set_aec_value(s, 300);    
  s->set_gain_ctrl(s, 1);      
  s->set_agc_gain(s, 0);      
  s->set_gainceiling(s, (gainceiling_t)0);  
  s->set_bpc(s, 0);            
  s->set_wpc(s, 1);           
  s->set_raw_gma(s, 1);        
  s->set_lenc(s, 1);           
  s->set_hmirror(s, 0);       
  s->set_vflip(s, 0);         
  s->set_dcw(s, 1);            
  s->set_colorbar(s, 0);       
}
