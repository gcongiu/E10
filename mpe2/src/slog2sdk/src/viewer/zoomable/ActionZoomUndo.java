/*
 *  (C) 2001 by Argonne National Laboratory
 *      See COPYRIGHT in top-level directory.
 */

/*
 *  @author  Anthony Chan
 */

package viewer.zoomable;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import viewer.common.Dialogs;

public class ActionZoomUndo implements ActionListener
{
    private ToolBarStatus      toolbar;
    private ModelTime          model;

    public ActionZoomUndo( ToolBarStatus in_toolbar, ModelTime in_model )
    {
        toolbar    = in_toolbar;
        model      = in_model;
    }

    public void actionPerformed( ActionEvent event )
    {
        Window  window;
        String  msg;

        if ( model.isZoomUndoStackEmpty() ) {
            window  = SwingUtilities.windowForComponent( (JToolBar) toolbar );
            msg     = "Zoom Undo Stack is empty";
            Dialogs.warn( window, msg );
        }
        else
            model.zoomUndo();

        // Set toolbar buttons to reflect status
        if ( toolbar != null )
            toolbar.resetZoomButtons();

        if ( Debug.isActive() )
            Debug.println( "Action for Zoom Undo button." );
    }
}
