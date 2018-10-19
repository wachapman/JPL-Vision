package org.team696.Commands;

import org.team696.autonomous.Command;
import org.team696.robot.Robot;

public class setelevator extends Command{
	double setPosition = 0.0;
	boolean movingUp;
	public setelevator(Double _position, Boolean _parallel){
		setPosition = _position;
		parallel = _parallel;
		movingUp = setPosition>Robot.elevator.getPosition();
		System.out.println("ELEVATOR CONSTRUCTOR   "+ setPosition + "   " + Robot.elevator.getPosition() + "   "+ movingUp);
	}
	@Override
	public void update(){
//		System.out.println(setPosition + "   " + Robot.elevator.getPosition() + "   "+ movingUp);
//		if(movingUp) Robot.elevator.setOverride(0.7);
//		else Robot.elevator.setOverride(-0.4);
//		//isFinished = Robot.elevator.atTarget();
//		isFinished = Math.abs(setPosition-Robot.elevator.getPosition())<0.2;
//		if(isFinished)Robot.elevator.setOverride(0.0);
		Robot.elevator.setPositon(setPosition);
		if(Robot.elevator.atTarget()){
			Robot.elevator.setOverride(0);
			isFinished = true;
		}
		System.out.println("ELEVATOR CODE   "+ setPosition + "   " + Robot.elevator.getPosition() + "   "+ movingUp);
	}
}
