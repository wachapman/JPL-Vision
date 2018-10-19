package org.team696.autonomous;

import org.team696.baseClasses.Runnable;
public class Command extends Runnable{
	
	protected boolean parallel = false;
	protected boolean isFinished;
	
	public Command(){
		parallel = false;
	}
	
	@Override
	public void update(){
	
	}
	public boolean finished(){
		return isFinished;
	}
	public boolean isParallel(){
		return parallel;
	}
}
