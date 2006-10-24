package palmthing.conduit;

import java.awt.*;

/* Copyright (c) 2001 Palm Inc. or its subsidiaries.  All rights reserved. */

/**
 * A panel with a border around it, similar to a checkbox group in C++.
 */

public class OptionsPanel extends Panel {

    /**
     * Overloaded function that draws the border around the panel.
     * 
     * @param g Graphics object to which this function can paint.
     */
    public void paint(Graphics g) {
        super.paint(g);
        g.setColor(new Color(200,200,200));
        g.draw3DRect(10, 20, getSize().width-20, getSize().height-35, false);
        g.draw3DRect(11, 21, getSize().width-21, getSize().height-36, true);
    }
    
}
