package eu.makestuff.fpgalink;

public class FLException extends RuntimeException {
	private static final long serialVersionUID = 1L;
	public final int RetCode;
	public FLException(String msg, int retCode) {
		super(msg);
		RetCode = retCode;
	}
}
