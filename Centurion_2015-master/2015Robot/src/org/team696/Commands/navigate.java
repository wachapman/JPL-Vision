package org.team696.Commands;

import org.team696.autonomous.Command;
import org.team696.baseClasses.*;
import org.team696.robot.Robot;

public class navigate extends Command{
	
	CustomPID speedController;
	CustomPID rotationController;
	int counter = 0;
	
	boolean finalWayPoint;
	double wayPointRadius;
	double speed;
	
	double[] navVector = {0.0,0.0,0.0};//x, y, and rotation
	double[] position = {0.0,0.0,0.0};
	double[] setVector = {0.0,0.0,0.0}; // speed, theta, and rotation
	
	public navigate(Double xGoal,Double yGoal,Double rotDegrees) {
		speed = 0.6;
		parallel = false;
		navVector[0] = xGoal;
		navVector[1] = yGoal;
		navVector[2] = rotDegrees;
		finalWayPoint = true;
		rotationController.setConstants(0.05, 0, 0.00);
		speedController.setConstants(0.1, 0, 0.00);
	}
	
	public navigate(Double xGoal,Double yGoal,Double rotDegrees,Double _speed) {
		speed = _speed;
		parallel = false;
		navVector[0] = xGoal;
		navVector[1] = yGoal;
		navVector[2] = rotDegrees;
		finalWayPoint = true;
		rotationController.setConstants(0.01, 0, 0.01);
		speedController.setConstants(0.01, 0, 0.01);
	}
	
	public navigate(Double xGoal,Double yGoal,Double rotDegrees,Double _speed, Double _waypointRadius) {
		speed = _speed;
		parallel = false;
		navVector[0] = xGoal;
		navVector[1] = yGoal;
		navVector[2] = rotDegrees;
		finalWayPoint = false;
		rotationController.setConstants(0.01, 0, 0.01);
		speedController.setConstants(0.01, 0, 0.01);
	}
	
	public navigate(Double xGoal,Double yGoal,Double rotDegrees,Double _speed, Double _waypointRadius, boolean _finalWayPoint) {
		speed = _speed;
		parallel = false;
		navVector[0] = xGoal;
		navVector[1] = yGoal;
		navVector[2] = rotDegrees;
		finalWayPoint = _finalWayPoint;
		rotationController.setConstants(0.01, 0, 0.0);
		speedController.setConstants(0.01, 0, 0.0);
	}
	
	public navigate(Double xGoal,Double yGoal,Double rotDegrees,Double _speed, Double _waypointRadius, Boolean _finalWayPoint, Boolean _parallel) {
		speed = _speed;
		parallel = _parallel;
		navVector[0] = xGoal;
		navVector[1] = yGoal;
		navVector[2] = rotDegrees;
		wayPointRadius = _waypointRadius;
		finalWayPoint = _finalWayPoint;
		rotationController = new CustomPID(0.03, 0, 0.01);
		speedController = new CustomPID(0.1, 0, 0.01);
	}
	
	@Override
	public void update(){
		position = Robot.drive.getPosition();
		
		setVector[1] = Math.toDegrees(Math.atan2(navVector[0]-position[0],navVector[1]-position[1]));
		double distance = Math.sqrt(Math.pow(navVector[0]-position[0], 2) + Math.pow(navVector[1]-position[1], 2));
		
		
		if(distance> wayPointRadius) setVector[0] = speed;
		else if((distance< wayPointRadius) && finalWayPoint){
			speedController.update(distance);
			setVector[0] = speedController.getOutput();
			if(distance<0.2){
				Robot.drive.setDriveValues(0, 0, 0, false);
				isFinished = true;
			}
		}
		else if(distance< wayPointRadius && !finalWayPoint){
			Robot.drive.setDriveValues(0, 0, 0, true);
			isFinished = true;
		}
		double error = navVector[2] - position[2];
		if(error>180) error = -(360-error);  //check if over the
		else if(error<-180) error = (360+error);//zero line to flip error 
		rotationController.update(error);
		setVector[2] = rotationController.getOutput();
		//System.out.println("NAVIGATION STUFF:   "+ navVector[0] + ",   " + position[0] +",   " +navVector[1] + ",   "+ position[1]);
		System.out.println("NAVX:   " + Robot.drive.getPosition()[2]);
		
		if(isFinished) Robot.drive.setDriveValues(0, 0, 0, false);
		else Robot.drive.setDriveValues(setVector[0], setVector[1], setVector[2], true);
		
	}
	
	
}
