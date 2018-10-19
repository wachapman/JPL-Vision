package org.team696.subsystems;

import org.team696.baseClasses.Runnable;

import edu.wpi.first.wpilibj.Solenoid;

public class AutoCanner extends Runnable {
	Solenoid leftGrab;
	Solenoid rightGrab;
	
	boolean leftOut = false;
	boolean rightOut = false;
	
	public AutoCanner(int leftSolenoid, int rightSolenoid) {
		leftGrab = new Solenoid(leftSolenoid);
		rightGrab = new Solenoid(rightSolenoid);
	}
	
	@Override
	public void start(int periodMS){
		super.start(periodMS);
	}
	
	@Override
	public void update() {
		run();
	}
	
	@Override
	public void stop(){
		super.stop();
	}
	
	public void set(boolean _leftOut,boolean _rightOut) {
		leftOut = _leftOut;
		rightOut = _rightOut;
	}
	
	public void run() {
		leftGrab.set(leftOut);
		rightGrab.set(rightOut);
	}
}