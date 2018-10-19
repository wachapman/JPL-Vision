package org.team696.Commands;

import org.team696.autonomous.Command;

import edu.wpi.first.wpilibj.Timer;

public class wait extends Command {
	Timer timer = new Timer();
	double waitTime = 0;
    public wait(Double _waitTime) {
    	waitTime = _waitTime;
	}
	
	@Override
	public void start(int periodMS){
		timer.reset();
		timer.start();
		parallel = false;
		super.start(periodMS);
	}
	@Override
	public void update(){
		if(timer.get()>waitTime) isFinished = true;
	}
	
}
