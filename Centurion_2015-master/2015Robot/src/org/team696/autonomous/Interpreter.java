package org.team696.autonomous;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Parameter;



public class Interpreter{
	String rawString = "";
	String parsedCode;
	String[] lines;
	int curLine = 0;
	
	public Interpreter(String code){
		rawString = code;
		lines =rawString.split("\n");
		
		for (int fish = 0; fish < lines.length; fish++) {
			lines[fish] = lines[fish].replace(" ", "");
		}
		
	}
	
	public boolean hasNextLine(){
		return curLine<lines.length;
	}
	
	public Command nextLine(){
		
		Class command = Command.class;
		
		Command newCommand = new Command();
		String[] splitCommand = lines[curLine].split(":");
		Object[] scriptArgs = {};
		
		if(splitCommand.length<2){
			
			try{
				command = Class.forName("org.team696.Commands."+splitCommand[0].toLowerCase());
				Constructor constructor = command.getDeclaredConstructor();
				newCommand = (Command) constructor.newInstance();
				
			}catch(ClassNotFoundException ex){ex.printStackTrace();}
			catch(NoSuchMethodException ex){ex.printStackTrace();}
			catch(InstantiationException ex){ex.printStackTrace();}
			catch(IllegalAccessException ex){ex.printStackTrace();}
			catch(InvocationTargetException ex){ex.printStackTrace();}
			
		}else{
			String[] argsString = splitCommand[1].split(",");
			//scriptArgs = new double[argsString.length];
			Object[] arguments = new Object[argsString.length];
			//System.out.println(argsString.length);
			for(int jesus = 0; jesus <argsString.length; jesus++){
				//scriptArgs[jesus] =  Double.parseDouble(argsString[jesus]);
				try{
					arguments[jesus] = Double.parseDouble(argsString[jesus]);
				}catch(NumberFormatException e){
					
					if(argsString[jesus].equalsIgnoreCase("true")) arguments[jesus] = true;
					else if(argsString[jesus].equalsIgnoreCase("false")) arguments[jesus] = false;
					else if(argsString[jesus].equalsIgnoreCase("parallel")) arguments[jesus] = true;
					else arguments[jesus] = false;
					
				}
			}
			Class[] classArg = new Class[arguments.length];
			
			for(int fish = 0; fish < arguments.length; fish++){
				classArg[fish] = arguments[fish].getClass();
			}
			
			try{
				command = Class.forName("org.team696.Commands."+splitCommand[0].toLowerCase());
				//classArg[classArg.length-1] = Boolean.class;
				
//				for(int fish = 0; fish < scriptArgs.length; fish++){
//					arguments[fish] = scriptArgs[fish];
//				}
				//arguments[arguments.length-1] = false;
				
				Constructor constructor = command.getDeclaredConstructor(classArg);
				//System.out.println(constructor.getParameterCount());
				newCommand = (Command) constructor.newInstance(arguments);
				
			}catch(ClassNotFoundException ex){ex.printStackTrace();}
			catch(NoSuchMethodException ex){ex.printStackTrace();}
			catch(InstantiationException ex){ex.printStackTrace();}
			catch(IllegalAccessException ex){ex.printStackTrace();}
			catch(InvocationTargetException ex){ex.printStackTrace();}
			
		}
		curLine ++;
		return newCommand;
	}
	
}
