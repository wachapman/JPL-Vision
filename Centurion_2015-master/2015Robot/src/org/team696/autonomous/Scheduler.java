package org.team696.autonomous;

import java.util.Enumeration;
import java.util.Vector;

import javax.print.attribute.standard.Finishings;
import org.team696.baseClasses.Runnable;

public class Scheduler extends Runnable{
	
	Vector<Command> curCommands = new Vector<Command>();
	Interpreter interpreter;
	public Scheduler(){
		interpreter = new Interpreter("wait:15.0");
	}
	
	public void setScript(String script){
		interpreter = new Interpreter(script);
	}
	
	public void addCommand(Command command,int periodMS){
		curCommands.addElement(command);
		curCommands.lastElement().start(periodMS);
		
	}
	
	@Override 
	public void start(int periodMS){
		super.start(periodMS);
	}
	@Override
	public void update(){
		boolean threadFree = true;
		int fish=0;
		while(fish<curCommands.size()){
			if(curCommands.get(fish).finished()){
				curCommands.get(fish).stop();
				curCommands.remove(fish);
			}else{
				if(!curCommands.get(fish).isParallel()) threadFree = false;
				fish++;
			}
		}
		
		if(threadFree && interpreter.hasNextLine()){
			curCommands.add(interpreter.nextLine());
			curCommands.lastElement().start(100);
			
		}
		
		if(curCommands.size()<1 && !interpreter.hasNextLine()){
			System.out.println("stopping");
			stop();
		}
	}
	
	@Override
	public void stop(){
		for(int fish=0;fish<curCommands.size();fish++){
			curCommands.elementAt(fish).stop();
		}
		super.stop();
	}
}
