● GPIO 8+9 → UART1 ✓ — na ESP32-C3 każdy GPIO może być remapowany do UART1 przez Serial1.begin(115200, SERIAL_8N1, rx, tx).                                                     
                                                                                                                                                                                
  Pliki audio — Edge TTS (bezpłatny, znakomity polski):                                                                                                                         
  pip install edge-tts                                                                                                                                                          
  edge-tts --voice pl-PL-MarekNeural --text "Dzienny limit wody osiągnięty. Wymagany reset." --write-media 001.mp3                                                              
  edge-tts --voice pl-PL-MarekNeural --text "Anomalia szybkości dolewania. Wymagany reset." --write-media 002.mp3                                                               
  edge-tts --voice pl-PL-MarekNeural --text "Limit dobowy i anomalia szybkości. Wymagany reset." --write-media 003.mp3                                                          
  edge-tts --voice pl-PL-MarekNeural --text "Krytycznie niski poziom wody. System zatrzymany. Uzupełnij zbiornik." --write-media 004.mp3                                        
  edge-tts --voice pl-PL-MarekNeural --text "Niski poziom wody w zbiorniku. Uzupełnij zbiornik." --write-media 005.mp3
  edge-tts --voice pl-PL-MarekNeural --text "<speak>Wykryto przycisk. <break time='600ms'/> Pięć. <break time='800ms'/> Cztery. <break time='800ms'/> Trzy. <break time='800ms'/> Dwa. <break time='800ms'/> Jeden. <break time='400ms'/> System gotowy do konfiguracji.</speak>" --write-media 006.mp3
  Upload: USB-C → komputer → DFPlayer Pro pojawia się jako pendrive → wgraj pliki w kolejności 001→006 (kolejność kopiowania = numer pliku).



                                                                                            
  Co się zmieniło:                                                                                                                                                              
                                                                                                                                                                                
  ┌─────────────────────┬────────────────────────────┬─────────────────────────────────────────────┐                                                                            
  │       Element       │           Przed            │                     Po                      │                                                                            
  ├─────────────────────┼────────────────────────────┼─────────────────────────────────────────────┤                                                                            
  │ Sygnalizacja błędów │ Pulsowanie GPIO 9 (buzzer) │ DFPlayer Pro — komunikaty głosowe           │                                                                            
  ├─────────────────────┼────────────────────────────┼─────────────────────────────────────────────┤                                                                          
  │ GPIO 9              │ ERROR_SIGNAL_PIN           │ DFPLAYER_TX_PIN (UART TX)                   │                                                                            
  ├─────────────────────┼────────────────────────────┼─────────────────────────────────────────────┤                                                                            
  │ GPIO 8              │ wolny                      │ DFPLAYER_RX_PIN (UART RX)                   │                                                                            
  ├─────────────────────┼────────────────────────────┼─────────────────────────────────────────────┤                                                                            
  │ Ostrzeżenie 1-3×    │ tylko log                  │ głosowy komunikat (plik 005) z powtarzaniem │                                                                          
  ├─────────────────────┼────────────────────────────┼─────────────────────────────────────────────┤                                                                            
  │ Alarm krytyczny 4×  │ tylko log + GPIO           │ głosowy komunikat (plik 004) z powtarzaniem │                                                                          
  └─────────────────────┴────────────────────────────┴─────────────────────────────────────────────┘                                                                            
                                                                                                                                                                              
  Pliki na DFPlayer (wgraj przez USB-C w tej kolejności):                                                                                                                       
  001.mp3 — dzienny limit wody (ErrorCode 1)                                                                                                                                  
  002.mp3 — anomalia szybkości (ErrorCode 2)                                                                                                                                    
  003.mp3 — limit + anomalia (ErrorCode 3)                                                                                                                                      
  004.mp3 — krytycznie niski poziom wody (ErrorCode 4)                                                                                                                          
  005.mp3 — niski poziom wody — ostrzeżenie (Warning)
  006.mp3 — provisioning — odliczanie + "system gotowy do konfiguracji"