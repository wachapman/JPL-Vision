/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package org.team696.baseClasses;

import java.util.Vector;

/**
 *
 * @author YoungJae
 */
public class Util {
    private static double out;
    public static double deadZone(double val, double lowVal, double highVal, double returnVal) {//y
        if ((val > lowVal)&&(val < highVal)) { 
            return (returnVal);
        }
        return (val);
    }
    public static double seperatedConstrain(double val,double minVal, double maxVal, double lowerBreak, double higherBreak, double center){
       if(val<center){
            return(constrain(map(val, minVal, center, minVal, lowerBreak),minVal,maxVal));
       }else if (val>center){
            return(constrain(map(val, center, maxVal, higherBreak, maxVal),minVal,maxVal));
       }else if (val == center){
            return(center);        
       }
       return(center);    
    }
    public static double smoothDeadZone(double val, double lowVal, double highVal, double lowerEnd, double higherEnd, double returnVal) {//y
        if ((val > lowVal)&&(val < highVal)) { 
            return (returnVal);
        }
        else if(val<lowVal){
            return(map(val,returnVal,lowerEnd,lowVal,lowerEnd));
        }else{
            return(map(val,returnVal,higherEnd,highVal,higherEnd));
        }
    }
    public static double map(double val, double lowIn, double highIn, double lowOut, double highOut) {//y
        return lerp(lowOut, highOut, norm(lowIn, highIn, val));
    }

    public static double norm(double low, double high, double input) {//y
        return ((input - low) / (high - low));
    }

    public static double lerp(double low, double high, double percent) {//y
        return (low + percent * (high - low));
        
    }
    public static double distBetween(double first, double second) {//y
        if (first < 0) {
            first *= -1;
        }
        if (second < 0) {
            second *= -1;
        }
        if (second < first) {
            return (first - second);
        }
        return (second - first);
    }
    
    public static double abs(double input){
        if(input < 0){
            input = -input;
        }
        return input;
    }
    
    public static double rocker(double val, double targVal) {//y

        if (val < 1) {
            return -targVal;
        } else if (val > 2) {
            return (targVal);
        } else {
            return (0);
        }
    }

    public static double constrain(double val, double lVal, double hVal) {//y
        if (val < lVal) {
            return lVal;
        } else if (hVal < val) {
            return hVal;
        }
        return val;
    }
    public static double smooth(double valWanted, double lastVal, double division){//y
        return(((valWanted-lastVal)/division)+lastVal);
    }
    public static double slowDown(double valWanted, double lastVal, double division){//y
        return(((valWanted-lastVal)/division));
    }
    public static double lowPass(double in, double a){
        out = (a * in) + out * (1-a);
        return(out);
    }
    public static double signOf(double num){
        if(num>=0){
            return(1);
        }
        return(-1);

    }
    public static double absDif(double one, double two){
        one = abs(one);
        two = abs(two);
        if(two>one){
            return(two-one);  
        }
        return(one-two);

    }
    public static String[] split(String input, String delimiter) {
        Vector node = new Vector();
        int index = input.indexOf(delimiter);
        while (index >= 0) {
            node.addElement(input.substring(0, index));
            input = input.substring(index + delimiter.length());
            index = input.indexOf(delimiter);
        }
        node.addElement(input);

        String[] retString = new String[node.size()];
        for (int i = 0; i < node.size(); ++i) {
            retString[i] = (String) node.elementAt(i);
        }

        return retString;
    }

    public static String removeChar(String input, String delimiter) {
        String[] splitArray = split(input, delimiter);
        String retString = "";
        for (int i = 0; i < splitArray.length; i++) {
            retString += splitArray[i];
        }
        return retString;
    }
}