/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package org.team696.baseClasses;
import org.team696.baseClasses.Util;
/**
 *
 * @author CMR
 */
public class CustomPID {
    private double setPoint;
    private double position;
    private double cumulativeError;
    private double previousError;
    private double error;
    private double kProportional;
    private double kIntegral;
    private double kDerivative;
    private double output;

    public CustomPID(double Kp, double Ki, double Kd){
        kProportional = Kp;
        kIntegral = Ki;
        kDerivative = Kd;
        setPoint = 0.0;
        position = 0.0;
        cumulativeError = 0.0;
        previousError = 0.0;
        error = 0.0;
        output = 0.0;
    }
    public void reset(){
        setPoint = 0.0;
        position = 0.0;
        cumulativeError = 0.0;
        previousError = 0.0;
        error = 0.0;
        output = 0.0;
    }
    
    public void setConstants(double Kp, double Ki, double Kd){
        kProportional = Kp;
        kIntegral = Ki;
        kDerivative = Kd;
    }
    public void update(double _error){
        error = _error;
        PID();
        previousError = error;
        
    }
    public double getOutput(){   
    //System.out.println("error   "+ error + "   " + output);
    return(output);
    }
    
    
    private void PID() 
    {
        output = (P()*kProportional) + (I()*kIntegral) + (D()*kDerivative);
    }
    private double P()
    {
        return(error);
    }
    private double I()
    {
        cumulativeError += error*0.3;
        cumulativeError = Util.constrain(cumulativeError, -9, 9);
        return(cumulativeError);
    }
    private double D()
    {
        return(error - previousError);
    }
    
}