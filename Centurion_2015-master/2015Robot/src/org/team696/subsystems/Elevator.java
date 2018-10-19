package org.team696.subsystems;

import org.team696.baseClasses.CustomPID;
import org.team696.baseClasses.Runnable;
import org.team696.baseClasses.Util;

import edu.wpi.first.wpilibj.DigitalInput;
import edu.wpi.first.wpilibj.Encoder;
import edu.wpi.first.wpilibj.Solenoid;
import edu.wpi.first.wpilibj.Timer;
import edu.wpi.first.wpilibj.VictorSP;

public class Elevator extends Runnable {
	
	public enum Presets{
		BOTTOM,
		ONE_TOTE_HIGH,
		TWO_TOTE_HIGH,
		TOP
	}
	
	public enum ControlType{
		TOTAL_OVERRIDE,
		OVERRIDE,
		POSITIONAL,
		PRESET
	}
	
	public Presets presets = Presets.BOTTOM;
	public ControlType controlType = ControlType.POSITIONAL;
	
	Encoder encoder;
	
	DigitalInput limitSwitchBot;
	DigitalInput limitSwitchTop;
	VictorSP elevMotor1;
	VictorSP elevMotor2;
	Solenoid brake;
	Intake intake;
	
	boolean intakeOpen = true;
	boolean intakeOverride = false;
	
	double target = 0.0;
	boolean totalOverride = false;
	
	double clicksPerTote = 0.00146484375;
	double curSetSpeed = 0.0;
	double inSetSpeed = 0.0;
	/*
	 * @param config - encoderSlotA, encoderSlotB, limitSwitchBot, limitSwitchTop, BreakerChannel
	 */
	public Elevator() {
		encoder = new Encoder(0, 1);
		encoder.setSamplesToAverage(10);
		
		limitSwitchBot = new DigitalInput(10);
		limitSwitchTop = new DigitalInput(11);

		brake = new Solenoid(4);
		intake = new Intake(6,5, 4, 1);
		elevMotor1 = new VictorSP(2);
		elevMotor2 = new VictorSP(3);
		encoder.setDistancePerPulse(clicksPerTote);
//		lastDistance = encoder.getDistance();
	}
	
	@Override
	public void start(int periodMS){
		intake.start(20);
		super.start(periodMS);
	}
	
	@Override
	public void update() {

		switch(controlType){
		case PRESET: 
			presetMotion();
			runElevator();
			break;
		case POSITIONAL: 
			positionalMotion();
			runElevator();
			break;
		case OVERRIDE:
//			target = encoder.getDistance();
			runElevator();
			break;
		case TOTAL_OVERRIDE:
//			target = encoder.getDistance();
			setSpeed(curSetSpeed);
			break;
		default:
			runElevator();
			break;
		}
	}

	public void setTotalOverride(double _speed){
		curSetSpeed = _speed;
		controlType = ControlType.TOTAL_OVERRIDE;
	}
	
	public void setOverride(double _speed){
		controlType = ControlType.OVERRIDE;
		inSetSpeed = _speed;
	}
	
	
	public void setPreset(Presets _preset){
		controlType = ControlType.PRESET;
		presets =_preset;
	}
	
	public void setPositon(double _position){
		controlType = ControlType.POSITIONAL;
		target = _position;
	}
	
	public void setIntakeMotors(double _speed){
		intake.setMotors(_speed);
	}
	public void setIntakeMotorsIndividual(double _leftSpeed, double _rightSpeed){
		intake.setIndividualMotors(_leftSpeed, _rightSpeed);
	}
	
	public void setIntakeOpen(boolean _open){
		intakeOpen = _open;
	}
	public void setEjector(boolean _eject){
		intake.setEjector(_eject);
	}	
	
	public boolean atTarget(){
		return Math.abs(target-getPosition())<0.2;
	}
	
	private void presetMotion(){
		
		switch(presets){
		case BOTTOM:		target = 0.05;break;
		case ONE_TOTE_HIGH:	target = 1;break;
		case TWO_TOTE_HIGH:	target = 2.9;break;
		case TOP:			target = 4;break;
		default:			target = 0;
		}
		positionalMotion();
	}
	
	private void positionalMotion(){
		double downSpeed = -0.8;
		//I did this so that we can clear the bottom tote because we move to slowly otherwise
		if(target < 1 && getPosition() < 1.5) downSpeed = -1;
		if(target-getPosition()>0.2)		setOverride(Util.constrain(target-getPosition(), downSpeed, 0.8));
		else if(target-getPosition()<0.2) 	setOverride(Util.constrain(target-getPosition(), downSpeed, 0.8));
		else 								setOverride(0);
	}

	
	private void runElevator(){
		
		boolean tempBottomSwitch = !limitSwitchBot.get();
		boolean tempTopSwitch = !limitSwitchTop.get();
		double tempDistance = encoder.getDistance();
		
		if(inSetSpeed <curSetSpeed) curSetSpeed += Util.constrain(inSetSpeed-curSetSpeed, -0.025, 0.025); 
		else curSetSpeed +=Util.constrain(inSetSpeed-curSetSpeed, -0.1, 0.1);
		
		if(curSetSpeed>0.01){            //if we are moving upwards
			if(tempDistance>4){
				if(tempTopSwitch){
					curSetSpeed=0;
					setSpeed(curSetSpeed);
				}
				else{
					curSetSpeed = Util.constrain(curSetSpeed,0, 0.3);
					setSpeed(curSetSpeed);
				}
			}else if(((tempDistance>0.2 && tempDistance<2.0) && !intake.isOpen())){
				curSetSpeed = 0;
				setSpeed(curSetSpeed, false);
			}
			else setSpeed(curSetSpeed);
			
			
		}else if(curSetSpeed<-0.01){                //if we are going downwards
			if(!intake.isOpen() && tempDistance<2.0 && tempDistance>0.2){
				curSetSpeed =0;
				setSpeed(curSetSpeed, false);
			}
			else if(tempDistance<0.5){
				curSetSpeed = Util.constrain(curSetSpeed, -0.3, 0);
				setSpeed(curSetSpeed);
			}
			else setSpeed(curSetSpeed);
		}else setSpeed(0);
		
		if((tempDistance<2.0 && tempDistance>0.2) && !intakeOverride) intakeOpen = true;
		
		intake.setOpen(intakeOpen);
		if(tempBottomSwitch || encoder.getDistance()<-0.1) encoder.reset();
	}
	
	public void toggleIntake(){
		intake.toggle();
	}
	
	public double getPosition(){
		return encoder.getDistance();
	}
	
//	public void setIntakeOverride(boolean _override){
//		intakeOverride = _override;
//	}
//	
	private void setSpeed(double _setspeed){
		if(Math.abs(_setspeed)<0.1){
			
			brake.set(true);
			elevMotor1.set(0);
			elevMotor2.set(0);
		}else{
			brake.set(false);
			elevMotor1.set(-_setspeed);
			elevMotor2.set(_setspeed);
		}	
	}
	
	private void setSpeed(double _setspeed, boolean _brake){
		brake.set(_brake);
		elevMotor1.set(-_setspeed);
		elevMotor2.set(_setspeed);
	}
	
	@Override
	public void stop(){
		super.stop();
		intake.stop();
	}
	
}