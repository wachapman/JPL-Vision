package org.team696.baseClasses;

import java.util.Timer;
import java.util.TimerTask;

public class Runnable {
	Timer updater = new Timer();
	TimerTask updateTask = new TimerTask() {
		@Override
		public void run() {
			update();
		}
	};
	public void update(){
		
	}
	public void start(int periodMS){
		updater = new Timer();
		updateTask = new TimerTask() {
			@Override
			public void run() {
				update();
			}
		};
		updater.schedule(updateTask, 1, periodMS);
		//schedule the update function
	}
	public void stop(){
		updater.cancel();
	}
	
	
}
