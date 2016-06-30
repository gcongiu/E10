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

public class ActionVportBackward implements ActionListener
{
    private ScrollbarTime   scrollbar;

    public ActionVportBackward( ScrollbarTime sb )
    {
        scrollbar = sb;
    }

    public void actionPerformed( ActionEvent event )
    {
        scrollbar.setValue( scrollbar.getValue()
                          - scrollbar.getBlockIncrement() / 2 );
        if ( Debug.isActive() )
            Debug.println( "Action for Backward Viewport button" );
    }
}
