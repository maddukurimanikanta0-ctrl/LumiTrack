"""
Streamlit Web Dashboard for LumiTrack (Variant 10.03: Museum Exposure Limiter)
Interactive visual monitor, polar light profile, cumulative exposure meter,
and canonical command interface for STM32 NUCLEO-F446RE.
"""

import streamlit as st
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import time
import sys
import os

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "02_Machine_Learning", "src"))

from host_controller import LumiTrackHostController
from feature_extraction import APPROVED_ANGLES_DEG

st.set_page_config(page_title="LumiTrack Museum Limiter", page_icon="🏛️", layout="wide")

st.title("🏛️ LumiTrack: Intelligent Light Mapping & Positioning System")
st.subheader("Parent Problem 10 | Variant 10.03: Museum Exposure Limiter")

# Initialize controller in session state
if "controller" not in st.session_state:
    st.session_state.controller = None
if "connected" not in st.session_state:
    st.session_state.connected = False
if "history" not in st.session_state:
    st.session_state.history = []

# Sidebar Controls
with st.sidebar:
    st.header("⚙️ Connection & Hardware")
    mode = st.radio("Operating Mode:", ["Simulation (Virtual NUCLEO-F446RE)", "Live Hardware (ST-LINK VCP)"])
    is_live = (mode == "Live Hardware (ST-LINK VCP)")
    
    if is_live:
        port = st.text_input("Serial Port:", value="COM3")
        baud = st.selectbox("Baud Rate:", [115200, 9600, 57600], index=0)
        scenario = "OVEREXPOSURE_RISK"
    else:
        port = None
        baud = 115200
        scenario = st.selectbox("Simulation Light Scenario:", ["OVEREXPOSURE_RISK", "NORMAL_DIFFUSE", "CONFUSER_SPECULAR"])
        
    col_conn, col_dis = st.columns(2)
    with col_conn:
        if st.button("🔌 Connect", disabled=st.session_state.connected):
            ctrl_mode = "live" if is_live else "sim"
            ctrl = LumiTrackHostController(mode=ctrl_mode, port=port, baudrate=baud, scenario=scenario)
            ctrl.connect()
            st.session_state.controller = ctrl
            st.session_state.connected = True
            st.rerun()
            
    with col_dis:
        if st.button("❌ Disconnect", disabled=not st.session_state.connected):
            if st.session_state.controller:
                st.session_state.controller.disconnect()
            st.session_state.controller = None
            st.session_state.connected = False
            st.rerun()
            
    st.divider()
    st.header("🕹️ Canonical Action Dispatcher")
    st.caption("Parent Problem 10 Canonical Contract:")
    
    col_c1, col_c2 = st.columns(2)
    with col_c1:
        if st.button("🔄 Full Scan", disabled=not st.session_state.connected):
            st.session_state.controller.trigger_scan(profile="FULL")
    with col_c2:
        if st.button("⚡ Partial Scan", disabled=not st.session_state.connected):
            st.session_state.controller.trigger_scan(profile="PARTIAL")
            
    col_c3, col_c4 = st.columns(2)
    with col_c3:
        if st.button("🏠 Safe Home", disabled=not st.session_state.connected):
            st.session_state.controller.return_home()
    with col_c4:
        if st.button("⏸️ Hold", disabled=not st.session_state.connected):
            st.session_state.controller.hold()
            
    st.subheader("Direct Approved Angle Choice")
    sel_angle_id = st.select_slider("Select Approved ANGLE_ID:", options=list(range(len(APPROVED_ANGLES_DEG))), format_func=lambda x: f"POS_{x} ({APPROVED_ANGLES_DEG[x]}°)")
    if st.button("🎯 Dispatch ANGLE_ID", disabled=not st.session_state.connected):
        st.session_state.controller.set_angle(sel_angle_id)

# Main Dashboard View
if st.session_state.connected and st.session_state.controller:
    telem = st.session_state.controller.latest_telemetry
    if telem:
        st.session_state.history.append({
            "timestamp": time.time(),
            "angle_deg": telem.get("angle_deg", 90),
            "lux": telem.get("lux", 0.0),
            "cum_lux": telem.get("cum_lux", 0.0),
            "state": telem.get("state", "SAFE_HOME")
        })
        if len(st.session_state.history) > 100:
            st.session_state.history.pop(0)
            
    # Metric Cards
    m1, m2, m3, m4, m5 = st.columns(5)
    with m1:
        st.metric("System State", telem.get("state", "SAFE_HOME") if telem else "OFFLINE")
    with m2:
        curr_deg = telem.get("angle_deg", 90) if telem else 90
        curr_id = telem.get("angle_id", 3) if telem else 3
        st.metric("Servo Position", f"{curr_deg}°", f"Angle ID {curr_id}")
    with m3:
        st.metric("Instant Light", f"{telem.get('lux', 0.0):.1f} Lux" if telem else "0.0 Lux")
    with m4:
        st.metric("Operator Threshold", f"{telem.get('target_lux', 500.0):.1f} Lux" if telem else "500.0 Lux")
    with m5:
        cum = telem.get('cum_lux', 0.0) if telem else 0.0
        pct = min(100.0, (cum / 50000.0) * 100.0)
        st.metric("Cumulative Exposure", f"{cum:.0f} Lux-s", f"{pct:.1f}% of limit")
        
    st.divider()
    
    # Graphs Row
    g1, g2 = st.columns([1, 1])
    with g1:
        st.subheader("🧭 Polar Light-Field Profile")
        scan_res = st.session_state.controller.latest_scan_result
        if scan_res and "data" in scan_res:
            pts = scan_res["data"]
            degs = [p["deg"] for p in pts]
            luxs = [p["lux"] for p in pts]
            
            fig = plt.figure(figsize=(6, 4))
            ax = fig.add_subplot(111, polar=True)
            rads = np.radians(degs)
            ax.plot(rads, luxs, 'ro-', linewidth=2, label="Scanned Profile")
            ax.fill(rads, luxs, 'red', alpha=0.2)
            ax.set_theta_zero_location('N')
            ax.set_title("Scanned Angular Intensity", va='bottom')
            ax.grid(True)
            st.pyplot(fig)
            plt.close()
        else:
            st.info("Trigger a Scan to map the angular light-field.")
            
    with g2:
        st.subheader("📈 Live Telemetry Trend")
        if len(st.session_state.history) > 2:
            hist_df = pd.DataFrame(st.session_state.history)
            st.line_chart(hist_df[["lux", "angle_deg"]])
        else:
            st.info("Accumulating live telemetry points...")
            
    # Recent Telemetry Packet Details
    with st.expander("🔍 Inspect Latest Raw MCU Frame", expanded=False):
        st.json(telem)

else:
    st.warning("⚠️ Controller is currently disconnected. Select mode and click 'Connect' in the sidebar to start.")
    st.markdown("""
    ### How to run:
    1. **Simulation Mode**: Run fully software-in-the-loop with the built-in virtual STM32 NUCLEO-F446RE emulator.
    2. **Live Hardware Mode**: Plug in your STM32 NUCLEO-F446RE board via USB, note the COM port in Windows Device Manager, and click Connect.
    """)
