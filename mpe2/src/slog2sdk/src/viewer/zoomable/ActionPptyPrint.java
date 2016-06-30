/*
 *  (C) 2001 by Argonne National Laboratory
 *      See COPYRIGHT in top-level directory.
 */

/*
 *  @author  Anthony Chan
 */

package viewer.zoomable;

import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;

public class ActionPptyPrint implements ActionListener
{
    // private TimelinePanel top_panel;

    public ActionPptyPrint( /* TimelinePanel panel */ )
    {
        // top_panel = panel;
    }

    public void actionPerformed( ActionEvent event )
    {
        if ( Debug.isActive() )
            Debug.println( "Action for Print Property button" );
    }
}
