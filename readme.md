Konzept 1:

Main-Task (Periodic)
	- Laufband
	- Scanner
	- Schieber

Ouswurf-Task (Periodic)
	- Lichtschranken
	- Auswürfe
	
	main_task {
		//Fließband steuern
		//Pakete scannen und rauslassen etc.
	}
	
	Parcel {
		uint8 current_position;
		uint8 ejection_position;
		uint8 asuwurf_flag;
		uint8 ejection_cooldown;
	}
	
	List<Parcel> parcels;
	uint8t[] ejection_cooldowns = [0, 0, 0]
	auswurf_task {
		for each parcel in parcels {
			parcel.current_position += DELTA
			if parcel.position >= parcel.ejection_position {
				eject(parcel.auswurf_flag)
				ejection_countdowns[parcel.auswurf_flag] = EJECTION_COOLDOWN
				parcels.remove(parcel)
			}
		}
		
		for i in [0, 1, 2] {
			if ejection_cooldowns[i] > 0 {
				ejection_cooldown[i]--;
			} else {
				reset_ejector(i)
			}
		}
	}
		
		
		



Konzept 2:

Einen Task für jeden Sensor
	- Sensor regestriert Paket
	- Sobald von HIGH to LOW
		- Signal an Auswurftask minimal zeitverzögert Auswerfen 
		
	Everyting as a Task
		
		auswurf_task {
			if ejection_scheduled {
				sleep(MINIMAL_DELAY_TO_CENTER)
				eject()
			}
		}
		
		sensor_task {
			for each sensor {
				if sensor.pervious_state == HIGH && sensor.current_state == LOW {
					signal(ejection_scheduled)
				}
			}
		}
		
		scanner_task {
			//Setzt Flags für die Steuerung des auswurf tasks
		}
		
		schieber_task {
			//abhängig von gescannt, sensor0 und sensor1
		}
		
		laufband0_task {
			
		}
		
		laufband1_task {
		
		}
		
		
			
	

Einen Task für jeden Auswurf
