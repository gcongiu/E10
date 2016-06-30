/*
 *  (C) 2001 by Argonne National Laboratory
 *      See COPYRIGHT in top-level directory.
 */

/*
 *  @author  Anthony Chan
 */

package viewer.zoomable;

// import java.awt.*;
import java.awt.event.*;
// import java.net.*;
// import javax.swing.*;

public class ActionYaxisTreeExpand implements ActionListener
{
    private ToolBarStatus      toolbar;
    private YaxisTree          tree_view;

    public ActionYaxisTreeExpand( ToolBarStatus   in_toolbar,
                                  YaxisTree       in_tree )
    {
        toolbar    = in_toolbar;
        tree_view  = in_tree;
    }

    public void actionPerformed( ActionEvent event )
    {
        if ( Debug.isActive() )
            Debug.println( "Action for Expand Tree button" );

        tree_view.expandLevel();
        toolbar.getYaxisTreeCommitButton().doClick();

        // Set toolbar buttons to reflect status
        toolbar.resetYaxisTreeButtons();
    }
}
