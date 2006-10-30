package palmthing.conduit;

import java.awt.*;

/* Based on code from samples in Graphic Java 1.2: Volume 1 AWT, by David Geary. */

/** 
 * Class that contains an image and allows it to be cached and added to
 * dialogs as a component.
 */
public class ImageCanvas extends Component {
    private Image image;

	public ImageCanvas() {
	}
	
    public ImageCanvas(Image image) {
		setImage(image);
    }
    
    public void paint(Graphics g) {
		if(image != null) {
        	g.drawImage(image, 0, 0, this);
		}
    }
    
    public void update(Graphics g) {
        paint(g);
    }
    
	public void setImage(Image image) {
        waitForImage(this, image);
		this.image = image;

        setSize(image.getWidth(this), image.getHeight(this));

		if(isShowing()) {
			repaint();
		}
	}
	
	public Dimension getMinimumSize() {
		if(image != null) {
			return new Dimension(image.getWidth(this),
		                     	image.getHeight(this));
		}
		else 
			return new Dimension(0,0);
	}

	public Dimension getPreferredSize() {
		return getMinimumSize();
	}

    public static void waitForImage(Component component, 
                                    Image image) {
        MediaTracker tracker = new MediaTracker(component);
        try {
            tracker.addImage(image, 0);
            tracker.waitForID(0);
        }
        catch(InterruptedException e) { e.printStackTrace(); }
    }
}
