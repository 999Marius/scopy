// Pluto loopback demo: TX a 10 kHz tone from the DDS, receive it on RX (I/Q)
// and show it in the Acquisition tool as a time plot and an FFT spectrum.
//
// Needs: a TX->RX cable, and a Scopy built with the `attr` API.
// Run:   ./build/scopy -s js/pluto_acq.js -r

// ---- Settings ------------------------------------------------------------------
var URI         = "ip:192.168.2.1"
var SAMPLE_RATE = "2400000"      // 2.4 MSPS: the FFT assumes 2.4 MHz
var LO          = "2400000000"   // 2.4 GHz, the same for RX and TX
var TX_GAIN     = "-30"          // dB, low because of the cable loopback
var RX_GAIN     = "20"           // dB, manual
var TONE_HZ     = "10000"        // 10 kHz: ~17 periods across the full buffer
var TONE_SCALE  = "0.5"
var BUFFER_SIZE = 4096           // samples per acquisition = FFT size
var PLOT_SIZE   = BUFFER_SIZE    // the plot window crops *every* plot, the FFT too,
                                 // so it must cover the whole buffer
var SPAN_HZ     = 100000         // spectrum shown from -SPAN_HZ to +SPAN_HZ

var DDS = "cf-ad9361-dds-core-lpc/"

function check(ok, what) {
    printToConsole((ok ? "OK    " : "FAIL  ") + what)
    return ok
}

// ---- 1. Connect ----------------------------------------------------------------
scopy.connectDevice(scopy.addDevice(URI))

// ---- 2. Radio ------------------------------------------------------------------
scopy.switchTool("AD936X")
msleep(500)
ad936x.setRxSamplingFrequency(SAMPLE_RATE)
ad936x.setTxSamplingFrequency(SAMPLE_RATE)
ad936x.setRxLoFrequency(LO)
ad936x.setTxLoFrequency(LO)
ad936x.setTxHardwareGain(0, TX_GAIN)
ad936x.setRxGainControlMode(0, "manual")
ad936x.setRxHardwareGain(0, RX_GAIN)
check(attr.read(URI, "ad9361-phy/altvoltage1_out/frequency") == LO, "TX LO = " + LO)

// ---- 3. TX tone (DDS) ----------------------------------------------------------
// One complex tone: I (altvoltage0) and Q (altvoltage2) 90 degrees apart.
// The second tones (altvoltage1, altvoltage3) are switched off.
function setTone(channel, phase) {
    return attr.write(URI, DDS + channel + "/frequency", TONE_HZ)
        && attr.write(URI, DDS + channel + "/scale", TONE_SCALE)
        && attr.write(URI, DDS + channel + "/phase", phase)
}
check(setTone("altvoltage0_out", "90000"), "DDS I tone")
check(setTone("altvoltage2_out", "0"), "DDS Q tone")
attr.write(URI, DDS + "altvoltage1_out/scale", "0")
attr.write(URI, DDS + "altvoltage3_out/scale", "0")
check(attr.write(URI, DDS + "altvoltage0_out/raw", "1"), "DDS on")

// ---- 4. Acquisition ------------------------------------------------------------
scopy.switchTool("Acquisition")
msleep(1000)

// RX I/Q are the device-controller channels ending in voltage0 / voltage1.
var rx = acq.getSourceChannels("dc").filter(function (ch) {
    return /:voltage[01]$/.test(ch)
})
if (!check(rx.length >= 2, "RX channels: " + rx))
    exit()

acq.stop()
acq.setSourceChannelEnabled("dc", rx[0], true)
acq.setSourceChannelEnabled("dc", rx[1], true)
acq.setMode("continuous")
acq.setBufferSize(BUFFER_SIZE)
acq.setPlotSize(PLOT_SIZE)

var keyI    = "dc_" + rx[0] + "_raw"
var keyQ    = "dc_" + rx[1] + "_raw"
var keyFft  = "genalyzer_spectrum_fft"
var keyFreq = "genalyzer_spectrum_freq"

// Two inputs = complex FFT. Inputs can only change while the engine is stopped.
check(acq.setBlockInputs("genalyzer-fft", keyI + "," + keyQ), "FFT inputs = I, Q")
check(acq.setBlockEnabled("genalyzer-fft", true), "FFT enabled")

// ---- 5. Plots ------------------------------------------------------------------
// Axis functions take the plot *channel* uid returned by addPlotChannel.
var timePlot = acq.addPlot("Time", "basic")
var chI = acq.addPlotChannel(timePlot, "curve", keyI, "")
var chQ = acq.addPlotChannel(timePlot, "curve", keyQ, "")
acq.setAxisAutoscale(chI, "y", true)
acq.setAxisAutoscale(chQ, "y", true)

// A pure tone is only a few bins wide, so zoom in around it instead of autoscaling.
var specPlot = acq.addPlot("Spectrum", "basic")
var chFft = acq.addPlotChannel(specPlot, "curve", keyFft, keyFreq)
acq.setAxisAutoscale(chFft, "x", false)
acq.setAxisAutoscale(chFft, "y", false)
acq.setAxisRange(chFft, "x", -SPAN_HZ, SPAN_HZ)
acq.setAxisRange(chFft, "y", -80, 60)

// ---- 6. Run --------------------------------------------------------------------
acq.run()
msleep(2000)
var fft = acq.getStats(keyFft, BUFFER_SIZE)
check(fft.max - fft.mean > 40, "tone visible: peak " + fft.max.toFixed(1) + " dB, mean " + fft.mean.toFixed(1) + " dB")
