sub flAwaitDevice {
	 my ($vp, $timeout, $isAvailable);
	 $vp = $_[0];
	 $timeout = $_[1];
	 $isAvailable = 0;
	 flSleep(1000);
	 while ( !$isAvailable && $timeout ) {
		  flSleep(100);
		  $isAvailable = flIsDeviceAvailable($vp);
		  $timeout--;
	 }
	 return $isAvailable;
}
1;
