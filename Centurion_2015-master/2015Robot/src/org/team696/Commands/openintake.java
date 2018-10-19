package org.team696.Commands;

import org.team696.autonomous.Command;
import org.team696.robot.Robot;

public class openintake extends Command{

	public openintake(){
		
	}
	
	@Override
	public void update() {
		Robot.elevator.setIntakeOpen(true);
		System.out.println("OPENINTAKE");
		isFinished = true;
	}
	
}
