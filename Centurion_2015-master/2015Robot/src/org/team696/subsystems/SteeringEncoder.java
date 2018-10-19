package org.team696.subsystems;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.UnsupportedEncodingException;

import org.team696.baseClasses.Runnable;
import org.team696.baseClasses.Util;

import edu.wpi.first.wpilibj.AnalogInput;
import edu.wpi.first.wpilibj.AnalogTrigger;
import edu.wpi.first.wpilibj.AnalogTriggerOutput;
import edu.wpi.first.wpilibj.Counter;
import edu.wpi.first.wpilibj.AnalogTriggerOutput.AnalogTriggerType;
import edu.wpi.first.wpilibj.Encoder;


public class SteeringEncoder extends Runnable {
	public Counter steerCounter;
	public AnalogInput encoder;
	public AnalogTrigger turnTrigger;
	public double offset;
	private double voltage;
	public int countOffset;
	double minVoltage = 0;
	double maxVoltage = 5;
	double oldVoltage;
	double angle;
	double lastStopWatch = 0;
	double degreesPerRotation = 102.85714285714285714285714285714;
	int wheel;
	
	public SteeringEncoder(int channel, int _wheel) throws FileNotFoundException, UnsupportedEncodingException,IOException{
		encoder = new AnalogInput(channel);
		
		turnTrigger = new AnalogTrigger(encoder);
		turnTrigger.setLimitsVoltage(1.0, 4.0);
		turnTrigger.setFiltered(true);
		
		steerCounter = new Counter();
		steerCounter.setUpDownCounterMode();
		steerCounter.setUpSource(turnTrigger, AnalogTriggerType.kRisingPulse);
		steerCounter.setDownSource(turnTrigger, AnalogTriggerType.kFallingPulse);
		
		wheel = _wheel;
		
		offset = 0;
		
		
		offset = Util.map( encoder.getVoltage(), minVoltage, maxVoltage, 0, degreesPerRotation);
		
//		try{
//			offset = Double.parseDouble(str);
//		}catch(NumberFormatException e){
//			offset = 0;
//		}
		steerCounter.reset();
		
		String s = "0";
//		try{
//			 s = counter.read(1)[0];
//			 counter.closeReader();
//		}catch(IOException e){e.printStackTrace();}
		if (s == null)countOffset = 0;
		else countOffset = Integer.parseInt(s);
		countOffset=0;
		steerCounter.reset();
	}
	
	@Override
	public void start(int periodMS){
		voltage = encoder.getVoltage();
		oldVoltage = voltage;
		super.start(periodMS);
	}
	
	@Override
	public void update(){
		super.update();
		
		int temp = countOffset+steerCounter.get();
		
		voltage = encoder.getVoltage();
	}
	
	@Override
	public void stop(){
		super.stop();
	}
	
	public void trimCenter(double trim){
		offset+=trim;
	}
	

	public void writeOffset(){
		System.out.print(wheel + "  writing");
//		offset=offset%degreesPerRotation; //THIS IS OUR OFFFSET WHICH WE ARE CHANGING
		offset = Util.map( encoder.getVoltage(), minVoltage, maxVoltage, 0, degreesPerRotation);
		
		if (offset < 0)offset+=degreesPerRotation;
		System.out.println(offset);
		countOffset=0;
		steerCounter.reset();
	}
	
	public double getAngleDegrees(){
		angle = (((countOffset+ steerCounter.get())*degreesPerRotation + Util.map( encoder.getVoltage(), minVoltage, maxVoltage, 0, degreesPerRotation))- offset)%360;
		if(angle<0) angle+=360;
		return angle;
	}
	public double getRawVoltage(){
		return voltage;
	}
}
