package org.team696.Commands;

import org.team696.autonomous.Command;
import org.team696.robot.Robot;

public class closeintake extends Command {

	public closeintake(){
		
	}
	
	@Override
	public void update() {
		Robot.elevator.setIntakeOpen(false);
		System.out.println("CLOSEINTAKE");
		isFinished = true;
	}
}
