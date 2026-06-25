import socket
import tkinter as tk

# --- CONFIGURATIE ---
ESP32_IP = "192.168.1.51"  # <-- VERANDER DIT NAAR HET IP-ADRES VAN JOUW ESP32
ESP32_PORT = 4210

# Initialiseer UDP Socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Aanmaken van de GUI
root = tk.Tk()
root.title("Mecanum Robot Controller")
root.geometry("400x350")
root.configure(bg='#222222')

active_key = None

def send_udp(message):
    try:
        sock.sendto(message.encode(), (ESP32_IP, ESP32_PORT))
    except Exception as e:
        print(f"Fout bij verzenden: {e}")

# Dit zorgt voor de vloeiende 'heartbeat' stroom aan data
def send_loop():
    if active_key:
        send_udp(active_key)
    else:
        send_udp('X')
    root.after(50, send_loop) # Stuur elke 50ms een pakketje

def on_key_press(event):
    global active_key
    key = event.char.upper()
    if key in ['Z', 'S', 'A', 'E', 'Q', 'D']:
        active_key = key
        status_label.config(text=f"Rijden: {key}", fg="#00bcd4")

def on_key_release(event):
    global active_key
    key = event.char.upper()
    if active_key == key:
        active_key = None
        status_label.config(text="STATUS: GESTOPT", fg="#d32f2f")

# Handmatige V en K tweaks versturen vanuit Python
def submit_tweak():
    cmd = tweak_entry.get().strip()
    if cmd:
        send_udp(cmd)
        print(f"Tweak verzonden: {cmd}")
        tweak_entry.delete(0, tk.END)

# --- GUI Layout ---
title_label = tk.Label(root, text="Mecanum WiFi Control", font=("Arial", 16, "bold"), bg='#222222', fg='white')
title_label.pack(pady=10)

info_label = tk.Label(root, text="Klik in dit venster en gebruik:\nZ (Vooruit) | S (Achteruit)\nA (Links) | E (Rechts)\nQ (Draai L) | D (Draai R)", font=("Arial", 10), bg='#222222', fg='#aaaaaa')
info_label.pack(pady=5)

status_label = tk.Label(root, text="STATUS: GESTOPT", font=("Arial", 14, "bold"), bg='#222222', fg='#d32f2f')
status_label.pack(pady=15)

# Tweak invoerveld onderin - NU MET PADX EN PADY GEFIKST!
tweak_frame = tk.Frame(root, bg='#333333', padx=10, pady=10)
tweak_frame.pack(pady=10, fill=tk.X, side=tk.BOTTOM)

tweak_label = tk.Label(tweak_frame, text="Stuur tweak (bijv: V180 of LA-10):", bg='#333333', fg='white')
tweak_label.pack(side=tk.LEFT, padx=5)

tweak_entry = tk.Entry(tweak_frame, width=10, font=("Arial", 12))
tweak_entry.pack(side=tk.LEFT, padx=5)

tweak_button = tk.Button(tweak_frame, text="Verstuur", command=submit_tweak, bg='#4CAF50', fg='white', relief=tk.FLAT)
tweak_button.pack(side=tk.LEFT, padx=5)

# Toetsenbord koppelingen
root.bind("<KeyPress>", on_key_press)
root.bind("<KeyRelease>", on_key_release)

# Start de achtergrond loop en de GUI
root.after(50, send_loop)
root.mainloop()