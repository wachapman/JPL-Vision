package org.team696.subsystems;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.UnsupportedEncodingException;

import edu.wpi.first.wpilibj.Encoder;
import edu.wpi.first.wpilibj.PIDController;
import edu.wpi.first.wpilibj.Talon;
import edu.wpi.first.wpilibj.Timer;
import edu.wpi.first.wpilibj.Victor;
import edu.wpi.first.wpilibj.VictorSP;
import edu.wpi.first.wpilibj.AnalogPotentiometer;
import edu.wpi.first.wpilibj.smartdashboard.SmartDashboard;

import org.team696.baseClasses.CustomPID;
import org.team696.baseClasses.Runnable;
import org.team696.baseClasses.ModuleConfigs;
import org.team696.baseClasses.Util;

public class SwerveModule extends Runnable{
	CustomPID steerController;
	Victor steerMotor;
	Victor driveMotor;
	Encoder driveEncoder;
	ModuleConfigs configs;
	public SteeringEncoder steerEncoder;
	double[] odometryVector = {0.0,0.0};
	double lastEncoderCount = 0;
	double overrideSteer = 0;
	double overrideSpeed = 0;
	double setAngle;
	double lastAngle;
	double setSpeed;
	double speed;
	double wheelDiam = 3.0;
	double gearRatio = 30.0/44.0;
	double pulsePerRot = 256.0;
	double distPerClick =wheelDiam*Math.PI*gearRatio*(1.0/12.0)/pulsePerRot;
	double encoderCount = 0.0;
	boolean override = false;
	boolean setWheelToZero = false;
	
	public SwerveModule(ModuleConfigs _configs)throws FileNotFoundException, UnsupportedEncodingException,IOException{
		configs = _configs;
		steerMotor = new Victor(configs.kSteerMotor);
		driveMotor = new Victor(configs.kDriveMotor);
		steerEncoder = new SteeringEncoder(configs.kSteerEncoder,configs.kWheelNumber);
//		steerController = new CustomPID(0.05,0, 0.3);
		steerController = new CustomPID(0.03,0, 0.2);
		driveEncoder = new Encoder(configs.kDriveEncoderA, configs.kDriveEncoderB);
		driveEncoder.setDistancePerPulse(wheelDiam*Math.PI*gearRatio*(1.0/12.0)/pulsePerRot);
		}

	@Override
	public void start(int periodMS){
		super.start(periodMS);
		steerEncoder.start(10);
		Timer.delay(0.1);
		override = false;
		driveEncoder.reset();
	}
	
	@Override
	public void update(){
		super.update();
		double error = 0.0;
		boolean reverseMotor =  false;
		
		double angle = steerEncoder.getAngleDegrees();
		if(angle<0) angle = 360+angle;

		lastEncoderCount = encoderCount;
		encoderCount = driveEncoder.getDistance();
		odometryVector[0] += (encoderCount-lastEncoderCount)*Math.sin(Math.toRadians(angle));
		odometryVector[1] += (encoderCount-lastEncoderCount)*Math.cos(Math.toRadians(angle));
		
		error = setAngle - angle;
		if(!setWheelToZero){
			if(error>180) error = -(360-error);  //check if over the
			else if(error<-180) error = (360+error);//zero line to flip error 
			
			if(error > 90){
				error = error-180;
				reverseMotor = true;
			}else if(error<-90){
				error = error+180;
				reverseMotor = true;
			}
		}
		
		steerController.update(-error);
		if(override) steerMotor.set(overrideSteer);
		else steerMotor.set(steerController.getOutput());
		
		if(override) driveMotor.set(overrideSpeed);
		else if(reverseMotor) driveMotor.set(-setSpeed);
		else driveMotor.set(setSpeed);
	}
	
	@Override
	public void stop(){
		steerEncoder.stop();
		super.stop();
	}
	
	public void setValues(double _setSpeed, double _setAngleDegrees){
		setSpeed = _setSpeed;
		if(_setAngleDegrees <0) _setAngleDegrees +=360;
		setAngle = _setAngleDegrees;
	}
	
	public void setSteerPID(double P, double I, double D){
		steerController.setConstants(P, I, D);
	}
	
	public void override(boolean _override, double _overrideSteer, double _overrideSpeed){
		override = _override;
		overrideSteer = _overrideSteer;
		overrideSpeed = _overrideSpeed;
	}
	
	public void setToZero(boolean _setToZero){
		setWheelToZero = _setToZero;
	}
	
	public double[] getCumVector(){
		double[] vector = odometryVector.clone();
		
		odometryVector[0] = 0;
		odometryVector[1] = 0;
		
		return vector;
	}
}
