import pygame
import serial
import time
import serial.tools.list_ports

def get_write_value(buttons, x, y):
    """Pack controller data into 4 bytes for ESP32"""
    # ESP32 expects: [buttons_low, buttons_high, x_axis, y_axis]
    return bytes([
        buttons & 0xFF,           # Low byte of buttons
        (buttons >> 8) & 0xFF,     # High byte of buttons
        x & 0xFF,                  # X axis (signed byte)
        y & 0xFF                   # Y axis (signed byte)
    ])

# List available ports
ports = serial.tools.list_ports.comports()
print("Available ports:")
for port in ports:
    print(f"  {port.device} - {port.description}")

# Initialize serial connection to ESP32
print("\nAttempting to connect to ESP32...")
try:
    # Update this to match your ESP32's port
    # Common ESP32 ports: /dev/ttyUSB0, /dev/ttyACM0, COM3, COM4, etc.
    ser = serial.Serial("/dev/ttyUSB0", 115200, timeout=0.1)
    print("Successfully connected to ESP32!")
except Exception as e:
    print(f"Failed to connect: {e}")
    print("Please check the port name and try again")
    exit(1)

# Wait for ESP32 to initialize
time.sleep(2)

# Initialize pygame for controller input
pygame.init()

# Check for available joysticks
if pygame.joystick.get_count() == 0:
    print("No joystick found! Please connect a controller.")
    exit(1)

joystick = pygame.joystick.Joystick(0)
joystick.init()

# Print controller information
print("\nController connected!")
print(f"Name: {joystick.get_name()}")
print(f"Axes: {joystick.get_numaxes()}")
print(f"Buttons: {joystick.get_numbuttons()}")
print("\nReady to play! Press buttons on your controller.\n")

# Controller state variables
buttons = 0
axis_x = 0
axis_y = 0

old_buttons = 0
old_axis_x = 0
old_axis_y = 0

# Main loop
clock = pygame.time.Clock()
running = True

try:
    while running:
        # Process pygame events
        for event in pygame.event.get():
            # Check for quit
            if event.type == pygame.QUIT:
                running = False
                break
            
            # Button press events
            elif event.type == pygame.JOYBUTTONDOWN:
                button_num = event.button
                
                # Map controller buttons to N64 buttons
                if button_num == 0:  # A button (Cross on PS4, A on Xbox)
                    buttons |= (1 << 0)
                    print("Pressed A")
                
                elif button_num == 2:  # B button (Square on PS4, X on Xbox)
                    buttons |= (1 << 1)
                    print("Pressed B")
                
                elif button_num == 7:  # Start button
                    buttons |= (1 << 3)
                    print("Pressed Start")
                
                elif button_num == 4:  # L button (Left bumper)
                    buttons |= (1 << 10)
                    print("Pressed L")
                
                elif button_num == 5:  # R button (Right bumper)
                    buttons |= (1 << 11)
                    print("Pressed R")
                
                elif button_num == 3:  # C-Up (Y on Xbox, Triangle on PS4)
                    buttons |= (1 << 12)
                    print("Pressed C-Up")
                
                elif button_num == 10:  # C-Down (Right stick click)
                    buttons |= (1 << 13)
                    print("Pressed C-Down")
                
                elif button_num == 6:  # C-Left (Select/Back button)
                    buttons |= (1 << 14)
                    print("Pressed C-Left")
                
                elif button_num == 1:  # C-Right (B on Xbox, Circle on PS4)
                    buttons |= (1 << 15)
                    print("Pressed C-Right")
                
                # D-Pad (if using hat/POV)
                elif button_num == 11:  # D-Up
                    buttons |= (1 << 4)
                    print("Pressed D-Up")
                
                elif button_num == 12:  # D-Down
                    buttons |= (1 << 5)
                    print("Pressed D-Down")
                
                elif button_num == 13:  # D-Left
                    buttons |= (1 << 6)
                    print("Pressed D-Left")
                
                elif button_num == 14:  # D-Right
                    buttons |= (1 << 7)
                    print("Pressed D-Right")
            
            # Button release events
            elif event.type == pygame.JOYBUTTONUP:
                button_num = event.button
                
                if button_num == 0:  # A
                    buttons &= ~(1 << 0)
                    print("Released A")
                
                elif button_num == 2:  # B
                    buttons &= ~(1 << 1)
                    print("Released B")
                
                elif button_num == 7:  # Start
                    buttons &= ~(1 << 3)
                    print("Released Start")
                
                elif button_num == 4:  # L
                    buttons &= ~(1 << 10)
                    print("Released L")
                
                elif button_num == 5:  # R
                    buttons &= ~(1 << 11)
                    print("Released R")
                
                elif button_num == 3:  # C-Up
                    buttons &= ~(1 << 12)
                    print("Released C-Up")
                
                elif button_num == 10:  # C-Down
                    buttons &= ~(1 << 13)
                    print("Released C-Down")
                
                elif button_num == 6:  # C-Left
                    buttons &= ~(1 << 14)
                    print("Released C-Left")
                
                elif button_num == 1:  # C-Right
                    buttons &= ~(1 << 15)
                    print("Released C-Right")
                
                elif button_num == 11:  # D-Up
                    buttons &= ~(1 << 4)
                    print("Released D-Up")
                
                elif button_num == 12:  # D-Down
                    buttons &= ~(1 << 5)
                    print("Released D-Down")
                
                elif button_num == 13:  # D-Left
                    buttons &= ~(1 << 6)
                    print("Released D-Left")
                
                elif button_num == 14:  # D-Right
                    buttons &= ~(1 << 7)
                    print("Released D-Right")
            
            # Analog stick events
            elif event.type == pygame.JOYAXISMOTION:
                # X-axis (left stick horizontal)
                if event.axis == 0:
                    if abs(event.value) > 0.3:  # Dead zone
                        axis_x = int(event.value * 80)  # Scale to N64 range
                    else:
                        axis_x = 0
                
                # Y-axis (left stick vertical)
                elif event.axis == 1:
                    if abs(event.value) > 0.3:  # Dead zone
                        axis_y = int(event.value * -80)  # Invert and scale
                    else:
                        axis_y = 0
                
                # Z button (Left trigger on modern controllers)
                elif event.axis == 2:
                    if event.value > -0.5:  # Trigger pressed
                        buttons |= (1 << 2)
                        print("Pressed Z")
                    else:
                        buttons &= ~(1 << 2)
                        print("Released Z")
            
            # Hat/POV events (D-Pad on some controllers)
            elif event.type == pygame.JOYHATMOTION:
                hat_x, hat_y = event.value
                
                # Clear all D-pad bits
                buttons &= ~(0xF << 4)
                
                # Set D-pad bits based on hat position
                if hat_y == 1:  # Up
                    buttons |= (1 << 4)
                    print("D-Pad Up")
                elif hat_y == -1:  # Down
                    buttons |= (1 << 5)
                    print("D-Pad Down")
                
                if hat_x == -1:  # Left
                    buttons |= (1 << 6)
                    print("D-Pad Left")
                elif hat_x == 1:  # Right
                    buttons |= (1 << 7)
                    print("D-Pad Right")
        
        # Send data to ESP32 if changed
        if (buttons != old_buttons or 
            axis_x != old_axis_x or 
            axis_y != old_axis_y):
            
            # Create data packet
            data = get_write_value(buttons, axis_x, axis_y)
            
            # Send to ESP32
            bytes_written = ser.write(data)
            
            # Debug output
            print(f"Sent: Buttons=0x{buttons:04X}, X={axis_x:4d}, Y={axis_y:4d} ({bytes_written} bytes)")
            
            # Update old values
            old_buttons = buttons
            old_axis_x = axis_x
            old_axis_y = axis_y
        
        # Read any debug messages from ESP32
        if ser.in_waiting > 0:
            esp32_msg = ser.readline().decode('utf-8', errors='ignore').strip()
            if esp32_msg:
                print(f"ESP32: {esp32_msg}")
        
        # Limit update rate (40 Hz)
        clock.tick(40)

except KeyboardInterrupt:
    print("\nExiting...")

finally:
    # Cleanup
    ser.close()
    pygame.quit()
    print("Connection closed.")
