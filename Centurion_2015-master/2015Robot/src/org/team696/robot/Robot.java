
package org.team696.robot;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;

import org.team696.baseClasses.*;
import org.team696.subsystems.*;
import org.team696.subsystems.Elevator.Presets;
import org.team696.autonomous.Scheduler;

import edu.wpi.first.wpilibj.BuiltInAccelerometer;
import edu.wpi.first.wpilibj.IterativeRobot;
import edu.wpi.first.wpilibj.Joystick;
import edu.wpi.first.wpilibj.PowerDistributionPanel;
import edu.wpi.first.wpilibj.Solenoid;
import edu.wpi.first.wpilibj.Timer;
import edu.wpi.first.wpilibj.smartdashboard.SmartDashboard;

/**
 * The VM is configured to automatically run this class, and to call the
 * functions corresponding to each mode, as described in the IterativeRobot
 * documentation. If you change the name of this class or the package after
 * creating this project, you must also update the manifest file in the resource
 * directory.
 */
public class Robot extends IterativeRobot {
    /**
     * This function is run when the robot is first started up and should be
     * used for any initialization code.
     */			
	boolean 		calibrate 		= false;
	
	Joystick        controlBoard = new Joystick(0);
//	Joystick		xBoxController=new Joystick(1);
	Joystick 		joystick	 = new Joystick(1);	
	PowerDistributionPanel pdp = new PowerDistributionPanel();
	BuiltInAccelerometer accelerometer = new BuiltInAccelerometer();
//	public static SwerveModule testModule;
	public static SwerveDrive     drive;
	public static Elevator        elevator;	
	
	boolean clawOpen			= false;
	Solenoid claw				= new Solenoid(6);
	
	double rotation				= 0;
	double yAxis				= 0;
	double xAxis				= 0;
	boolean snapToFeederButton	= false;
	boolean fieldCentric 		= true;
	boolean fieldCentricButton	= false;
	boolean oldFieldCentricButton = fieldCentricButton;
	boolean tankDriveSwitch		= false;
	
	boolean setWheelsToZero		= false;
	boolean slowDownButton		= false;
	boolean intakeWheelsIn		= false;
	boolean intakeWheelsOut		= false;
	boolean intakeOverrideSwitch= false;
	boolean closeIntakeButton	= false;
	double elevatorStick		= 0.0;
	boolean elevatorTotalOverrideSwitch = false;
	
	boolean presetButtonBottom = false;
	boolean presetButtonOneToteHigh = false;
	boolean presetButtonTwoToteHigh = false;
	boolean presetButtonTop = false;
	
	boolean zeroOdometryButton = false;
	
	boolean zeroNavXButton = false;
	
	double          trim            = 0;
	boolean			write			= false;
	boolean			oldWrite		= write;
	ModuleConfigs[] configs;
	
//	Solenoid intakeOpen = new Solenoid(5);
	
	Scheduler autonScheduler = new Scheduler();
	
	
	public void setConfig(){
		configs         = new ModuleConfigs[4];
		for(int fish=0;fish<4;fish++){
			configs[fish] = new ModuleConfigs();
		}
		configs[0].kSteerMotor     = 16;
		configs[0].kDriveMotor     = 0;
		configs[0].kSteerEncoder   = 2;
		configs[0].kDriveEncoderA  = 7;
		configs[0].kDriveEncoderB  = 6;
		configs[0].kWheelNumber    = 1;
		configs[0].kReverseEncoder = false;
		configs[0].kReverseMotor   = false;
		
		configs[1].kSteerMotor     = 6;
		configs[1].kDriveMotor     = 5;
		configs[1].kSteerEncoder   = 1;
		configs[1].kDriveEncoderA  = 5;
		configs[1].kDriveEncoderB  = 4;
		configs[1].kWheelNumber    = 2;
		configs[1].kReverseEncoder = false;
		configs[1].kReverseMotor   = false;
		
		configs[2].kSteerMotor     = 7;
		configs[2].kDriveMotor     = 8;
		configs[2].kSteerEncoder   = 0;
		configs[2].kDriveEncoderA  = 3;
		configs[2].kDriveEncoderB  = 2;
		configs[2].kWheelNumber    = 3;
		configs[2].kReverseEncoder = false;
		configs[2].kReverseMotor   = false;

		configs[3].kSteerMotor     = 17;
		configs[3].kDriveMotor     = 18;
		configs[3].kSteerEncoder   = 3;
		configs[3].kDriveEncoderA  = 9;
		configs[3].kDriveEncoderB  = 8;
		configs[3].kWheelNumber    = 4;
		configs[3].kReverseEncoder = false;
		configs[3].kReverseMotor   = false;
		
	}
	
	public String getDate(){
		Date date = new Date();
		DateFormat df = new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss");
		return df.format(date);
	}
	
	public void robotInit(){		
		elevator = new Elevator();
		setConfig();
		try {
			drive = new SwerveDrive(configs);
			drive.frontLeft.steerEncoder.start(10);
			drive.frontRight.steerEncoder.start(10);
			drive.backRight.steerEncoder.start(10);
			drive.backLeft.steerEncoder.start(10);
//			testModule = new SwerveModule(configs[2]);
		} 
		catch(FileNotFoundException fnfE){}
		catch(IOException ioE){}
		
    }

    /**
     * This function is called periodically during autonomous
     */
	@Override
	public void autonomousInit(){
		
    	elevator.start(10);
    	drive.start(10);
    	drive.zeroNavX();
		String autonScript = SmartDashboard.getString("autonCode", "StringNotFound");
		System.out.println(autonScript);
		autonScheduler = new Scheduler();
		autonScheduler.setScript(autonScript);
		autonScheduler.stop();
		autonScheduler.start(20);
		
	}
	
	
	
    public void autonomousPeriodic() {

    }

    @Override
    public void teleopInit() {
        	//setting fieldcentric to false for comfortability
    	fieldCentric = false;
    	drive.stop();
    	elevator.stop();
    	autonScheduler.stop();
    	System.out.println("stopping scheduler");
    	drive.start(10);
    	drive.zeroNavX();
    	drive.yawOffset = 90;
//    	testModule.start(10);
    	elevator.start(10);
    }
    
    public void teleopPeriodic() {
    	
    	
    	//xbox controller
//    	rotation			= Util.deadZone(xBoxController.getRawAxis(4), -0.1, 0.1, 0)/5;
//    	yAxis				= -Util.deadZone(Util.map(xBoxController.getRawAxis(1), -1, 1, 1.5, -1.5),-0.1,0.1,0);
//    	xAxis				= Util.deadZone(Util.map(xBoxController.getRawAxis(0), -1, 1, 1.5, -1.5),-0.1,0.1,0);
    	
    	//control board and joystick
    	yAxis				= Util.deadZone(Util.map(joystick.getRawAxis(1), -1, 1, -1.5, 1.5),-0.1,0.1,0);
    	xAxis				= -Util.deadZone(Util.map(joystick.getRawAxis(0), -1, 1, -1.5, 1.5),-0.1,0.1,0);
    	rotation			= Util.deadZone(controlBoard.getRawAxis(0), -0.1, 0.1, 0)/2;
    	System.out.println("y: " + yAxis + "     " + "x: " + xAxis + "      " + "r: " + rotation + "    ");
    	
    	//all control board
//    	yAxis				= -Util.deadZone(Util.map(controlBoard.getRawAxis(1), -1, 1, 1.5, -1.5),-0.1,0.1,0);
//    	xAxis				= Util.deadZone(Util.map(controlBoard.getRawAxis(2), -1, 1, 1.5, -1.5),-0.1,0.1,0);
    	
    	snapToFeederButton	= false;//controlBoard.getRawButton(1);
    	oldFieldCentricButton = fieldCentricButton;
    	fieldCentricButton	= controlBoard.getRawButton(7);
    	setWheelsToZero		= controlBoard.getRawButton(4);
    	slowDownButton		= false;//controlBoard.getRawButton(1);
    	tankDriveSwitch		= controlBoard.getRawButton(2);
    	
    	intakeWheelsIn		= controlBoard.getRawAxis(3)<-0.5;
    	intakeWheelsOut		= controlBoard.getRawAxis(3)>0.5;
    	intakeOverrideSwitch= false;//controlBoard.getRawButton(0);
    	closeIntakeButton	= controlBoard.getRawButton(6);
    	elevatorStick		= controlBoard.getRawAxis(4);
    	elevatorTotalOverrideSwitch = false;//controlBoard.getRawButton(5);
    	
    	presetButtonBottom = controlBoard.getRawButton(13);
    	presetButtonOneToteHigh = controlBoard.getRawButton(12);
    	presetButtonTwoToteHigh = controlBoard.getRawButton(11);
    	presetButtonTop 		= controlBoard.getRawButton(10);
    	
    	zeroOdometryButton = controlBoard.getRawButton(8);
    	
    	zeroNavXButton = controlBoard.getRawButton(9);
    	
    	calibrate = controlBoard.getRawButton(5);
    	if(calibrate) calibrate();
    	else robotCode();
    }
    
    public void calibrate(){
    	
    	oldWrite = write;
    	write = closeIntakeButton;
    	trim = elevatorStick*2;
    	if(setWheelsToZero){
    		drive.frontLeft.setToZero(true);
    		drive.frontRight.setToZero(true);
    		drive.backRight.setToZero(true);
    		drive.backLeft.setToZero(true);
        }else{
        	drive.frontLeft.setToZero(false);
    		drive.frontRight.setToZero(false);
    		drive.backRight.setToZero(false);
    		drive.backLeft.setToZero(false);
        
    	if(presetButtonTop){
    		//drive.frontLeft.steerEncoder.trimCenter(trim);
    		drive.frontLeft.override(true, elevatorStick, 0.1);
    		if(write && !oldWrite) drive.frontLeft.steerEncoder.writeOffset();
    	}
    	else{
    		//drive.frontLeft.steerEncoder.trimCenter(0);
    		drive.frontLeft.override(true, 0, 0);
    	}
  
    	if(presetButtonTwoToteHigh){
    		//drive.frontRight.steerEncoder.trimCenter(trim);
    		drive.frontRight.override(true, elevatorStick, 0.1);
    		if(write && !oldWrite) drive.frontRight.steerEncoder.writeOffset();
    	}
    	else{
    		//drive.frontRight.steerEncoder.trimCenter(0);
    		drive.frontRight.override(true, 0, 0);
        	
    	}
    	
    	if(presetButtonOneToteHigh){
    		//drive.backRight.steerEncoder.trimCenter(trim);
    		drive.backRight.override(true, elevatorStick, 0.1);
    		if(write && !oldWrite) drive.backRight.steerEncoder.writeOffset();
    	}
    	else {
    		//drive.backRight.steerEncoder.trimCenter(0);
    		drive.backRight.override(true, 0, 0);
    	}
    	
    	if(presetButtonBottom){
    		//drive.backLeft.steerEncoder.trimCenter(trim);
    		drive.backLeft.override(true, elevatorStick, 0.1);
    		if(write && !oldWrite) drive.backLeft.steerEncoder.writeOffset();
    	}
    	else{
    		//drive.backLeft.steerEncoder.trimCenter(0);
    		drive.backLeft.override(true, 0, 0);
        	
    	}
        }
    }
    
    public void robotCode(){
//    	logger.set(controlBoard.getRawButton(2), 4);
//    	logger.set(pdp.getVoltage(), 3);
    	
    	if(controlBoard.getRawButton(8))clawOpen = false;
    	else if (controlBoard.getRawButton(9))clawOpen = true;
    	claw.set(clawOpen);
    	
    	System.out.print("elev pos: "+ elevator.getPosition());
    	
    	if(zeroNavXButton) drive.zeroNavX();
    	
    	if(fieldCentricButton&& !oldFieldCentricButton) fieldCentric = !fieldCentric;
    	
    	double angle;
    	if(Math.abs(xAxis)<0.1 && Math.abs(yAxis)<0.1) angle = 0;
    	else  angle = Math.toDegrees(-Math.atan2(xAxis, -yAxis));
    	if(angle<0) angle+=360;
    	
    	drive.setETankMode(tankDriveSwitch);
    	if(!tankDriveSwitch){
    		drive.frontLeft.override(false, 0, 0); //TURN OFF OVERRIDES
    		drive.frontRight.override(false, 0, 0);
    		drive.backRight.override(false, 0, 0);
    		drive.backLeft.override(false, 0, 0);
    	}
    	
//    	if(snapToFeederButton) drive.alignFeeder();
//    	else if(slowDownButton)drive.setDriveValues(Math.sqrt((yAxis*yAxis)+(xAxis*xAxis))/3, angle, rotation, fieldCentric);
    	
    	drive.setDriveValues(Math.sqrt((yAxis*yAxis)+(xAxis*xAxis))/2, angle, rotation*3, fieldCentric);
       	
//    	elevator.setIntakeOverride(controlBoard.getRawButton(6));
    	elevator.setIntakeOpen(!closeIntakeButton);
    	if(intakeWheelsIn){
//    		elevator.setIntakeMotors(1.0+ xBoxController.getRawAxis());
//    		elevator.setIntakeMotorsIndividual(1.0-(xBoxController.getRawAxis(3)/2), 1.0-(xBoxController.getRawAxis(2)/2));
    		elevator.setIntakeMotors(1);
    	}
    	else if(intakeWheelsOut) elevator.setIntakeMotors(-1.0);
    	else elevator.setIntakeMotors(0);
    	
    	//elevator.setEjector(ejectButton);
    	if(zeroOdometryButton) drive.zeroOdometry();
    	if(elevatorTotalOverrideSwitch)			elevator.setTotalOverride(elevatorStick);
    	else if(presetButtonBottom)				elevator.setPreset(Presets.BOTTOM);
    	else if(presetButtonOneToteHigh)		elevator.setPreset(Presets.ONE_TOTE_HIGH);
    	else if(presetButtonTwoToteHigh)		elevator.setPreset(Presets.TWO_TOTE_HIGH);
    	else if(presetButtonTop)				elevator.setPreset(Presets.TOP);
    	else 									elevator.setOverride(elevatorStick);
//    	System.out.println(drive.getPosition()[0]+ "   " + drive.getPosition()[1] + "   " + drive.getPosition()[2]);
    	
//    	System.out.print((int)drive.frontLeft.steerEncoder.offset+ "   ");
//    	System.out.print((int)drive.frontRight.steerEncoder.offset+ "   ");
//    	System.out.print((int)drive.backLeft.steerEncoder.offset+ "   ");
//    	System.out.print((int)drive.backLeft.steerEncoder.offset+ "   ");
//    	
//    	System.out.print((int)drive.frontLeft.steerEncoder.countOffset+ "   ");
//    	System.out.print((int)drive.frontRight.steerEncoder.countOffset+ "   ");
//    	System.out.print((int)drive.backLeft.steerEncoder.countOffset+ "   ");
//    	System.out.print((int)drive.backLeft.steerEncoder.countOffset+ "   ");
//    	
//    	System.out.print((int)drive.frontLeft.steerEncoder.getAngleDegrees()+ "   ");
//    	System.out.print((int)drive.frontRight.steerEncoder.getAngleDegrees()+ "   ");
//    	System.out.print((int)drive.backLeft.steerEncoder.getAngleDegrees()+ "   ");
//    	System.out.println((int)drive.backLeft.steerEncoder.getAngleDegrees()+ "   ");
    	
//    	System.out.println(drive.getPosition()[0]+ "   " + drive.getPosition()[1]+ "   " + drive.getPosition()[2]);    	
    	
//    	testModule.setValues(Math.sqrt((yAxis*yAxis)+(xAxis*xAxis))/2, angle);
//    	testModule.override(controlBoard.getRawButton(2), controlBoard.getRawAxis(2));

    }
    
    /**
     * This function is called periodically during test mode
     */
    public void testPeriodic() {
    
    }
    
    @Override
    public void disabledInit() {
//    	testModule.stop();
    	autonScheduler.stop();
    	drive.stop();
    	elevator.stop();
    }
}