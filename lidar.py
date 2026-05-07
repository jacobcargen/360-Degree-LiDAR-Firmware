#!/usr/bin/env python3
import asyncio, struct, math, time, threading
import pygame
from bleak import BleakClient, BleakScanner

CHAR_LIDAR_UUID = "12345678-1234-1234-1234-123456789abd"
CHAR_CMD_UUID   = "12345678-1234-1234-1234-123456789abe"
DEVICE_NAME     = "LiDAR360"

RADAR_SIZE = 1000
PANEL_W    = 210
WIN_W      = RADAR_SIZE + PANEL_W
WIN_H      = RADAR_SIZE
CENTER     = RADAR_SIZE // 2
FPS        = 60

points_lock   = threading.Lock()
points        = {}
current_angle = 0.0
ble_status    = "Scanning..."
sweep_id      = 0
last_angle    = 0.0
last_delta    = 0.0
ble_client    = None
cmd_queue     = []
cmd_lock      = threading.Lock()

def send_cmd(cmd):
    with cmd_lock:
        cmd_queue.append(cmd)

def strength_to_color(s):
    t = min(max(s / 30.0, 0.05), 1.0)
    return (int(180 * t), int(100 + 155 * t), int(60 * t))

def polar_to_screen(angle, dist, max_range):
    rad = math.radians(angle)
    scale = (CENTER - 40) / (max_range * 100)
    return int(CENTER + dist * math.cos(rad) * scale), int(CENTER - dist * math.sin(rad) * scale)

def on_lidar_data(sender, data):
    global current_angle, sweep_id, last_angle, last_delta
    if len(data) != 12:
        return
    angle, distance, strength = struct.unpack("<fff", data)
    delta = angle - last_angle
    if delta > 180: delta -= 360
    if delta < -180: delta += 360
    if last_delta != 0.0 and abs(delta) > 0.01 and (delta > 0) != (last_delta > 0):
        sweep_id += 1
    if abs(delta) > 0.01:
        last_delta = delta
    last_angle = angle
    bucket = int(angle / 0.5) % 720
    with points_lock:
        points[bucket] = (angle, distance, strength, sweep_id)
    current_angle = angle

async def ble_loop():
    global ble_status, ble_client
    while pygame.get_init():
        try:
            ble_status = "Scanning..."
            device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=15)
            if not device:
                ble_status = "Not found"
                await asyncio.sleep(2)
                continue
            ble_status = "Connecting..."
            async with BleakClient(device, timeout=15) as client:
                ble_client = client
                ble_status = "Connected"
                await client.start_notify(CHAR_LIDAR_UUID, on_lidar_data)
                for cmd in ["G20", "F1000"]:
                    try:
                        await client.write_gatt_char(CHAR_CMD_UUID, cmd.encode(), response=False)
                    except Exception:
                        pass
                while pygame.get_init() and client.is_connected:
                    with cmd_lock:
                        pending = list(cmd_queue)
                        cmd_queue.clear()
                    for cmd in pending:
                        try:
                            await client.write_gatt_char(CHAR_CMD_UUID, cmd.encode(), response=False)
                        except Exception:
                            pass
                    await asyncio.sleep(0.05)
                try:
                    await client.stop_notify(CHAR_LIDAR_UUID)
                except Exception:
                    pass
                ble_client = None
        except Exception:
            ble_status = "Disconnected"
        if pygame.get_init():
            await asyncio.sleep(2)

def run_ble():
    asyncio.run(ble_loop())


class Button:
    def __init__(self, x, y, w, h, label, toggle=False, danger=False):
        self.rect   = pygame.Rect(x, y, w, h)
        self.label  = label
        self.toggle = toggle
        self.danger = danger
        self.active = False

    def draw(self, surface, font, mouse_pos):
        hovered = self.rect.collidepoint(mouse_pos)
        if self.danger:
            bg, border = (220, 70, 70) if hovered else (180, 50, 50), (220, 70, 70)
        elif self.toggle:
            bg     = (50, 70, 50) if self.active else (45, 45, 50)
            border = (0, 180, 80) if self.active else (120, 120, 120)
        else:
            bg, border = (50, 70, 50) if hovered else (35, 50, 35), (0, 200, 0)
        pygame.draw.rect(surface, bg, self.rect, border_radius=6)
        pygame.draw.rect(surface, border, self.rect, 1, border_radius=6)
        color = (0, 200, 0) if (self.active or not self.toggle) else (120, 120, 120)
        lbl = font.render(self.label, True, color)
        surface.blit(lbl, (self.rect.x + (self.rect.w - lbl.get_width()) // 2,
                            self.rect.y + (self.rect.h - lbl.get_height()) // 2))

    def clicked(self, pos):
        return self.rect.collidepoint(pos)


class Slider:
    def __init__(self, x, y, w, label, min_val, max_val, value, step=1, fmt="{}"):
        self.x, self.y, self.w = x, y, w
        self.label   = label
        self.min_val = min_val
        self.max_val = max_val
        self.value   = value
        self.step    = step
        self.fmt     = fmt

    def draw(self, surface, font):
        lbl = font.render(self.label, True, (140, 160, 140))
        val = font.render(self.fmt.format(self.value), True, (0, 220, 80))
        surface.blit(lbl, (self.x, self.y - 18))
        surface.blit(val, (self.x + self.w - val.get_width(), self.y - 18))
        pygame.draw.rect(surface, (30, 30, 35), (self.x, self.y, self.w, 12), border_radius=6)
        fill_w = int(self.w * (self.value - self.min_val) / max(self.max_val - self.min_val, 1))
        pygame.draw.rect(surface, (0, 160, 60), (self.x, self.y, fill_w, 12), border_radius=6)
        pygame.draw.circle(surface, (0, 220, 80), (self.x + fill_w, self.y + 6), 7)
        pygame.draw.circle(surface, (18, 18, 22), (self.x + fill_w, self.y + 6), 4)

    def update(self, mouse_x):
        frac       = max(0.0, min(1.0, (mouse_x - self.x) / self.w))
        self.value = round((self.min_val + frac * (self.max_val - self.min_val)) / self.step) * self.step
        self.value = max(self.min_val, min(self.max_val, self.value))

    def hit_test(self, pos):
        return self.x - 5 <= pos[0] <= self.x + self.w + 5 and self.y - 5 <= pos[1] <= self.y + 20


def main():
    pygame.init()
    screen   = pygame.display.set_mode((WIN_W, WIN_H))
    pygame.display.set_caption("LiDAR 360")
    clock    = pygame.time.Clock()
    font_sm  = pygame.font.SysFont("monospace", 13)
    font_med = pygame.font.SysFont("monospace", 15, bold=True)
    font_lg  = pygame.font.SysFont("monospace", 20, bold=True)

    threading.Thread(target=run_ble, daemon=True).start()

    px  = RADAR_SIZE + 10
    pw  = PANEL_W - 20
    bw2 = (pw - 8) // 2
    bh  = 28

    btn_enable  = Button(0, 0, bw2, bh, "Enable",  toggle=True)
    btn_disable = Button(0, 0, bw2, bh, "Disable", toggle=True)
    btn_disable.active = True
    btn_auto    = Button(0, 0, bw2, bh, "Auto",    toggle=True)
    btn_manual  = Button(0, 0, bw2, bh, "Manual",  toggle=True)
    btn_auto.active = True
    btn_left    = Button(0, 0, bw2, bh, "Left")
    btn_right   = Button(0, 0, bw2, bh, "Right")
    btn_home    = Button(0, 0, pw,  bh, "Set Home")
    btn_stop    = Button(0, 0, pw,  bh, "Stop and Quit", danger=True)

    range_slider    = Slider(0, 0, pw, "Range (m)",       0.1, 12.0, 4.0,  step=0.1, fmt="{:.1f}m")
    speed_slider    = Slider(0, 0, pw, "Speed (us/step)", 500, 20000, 2600, step=100, fmt="{}us")
    jog_slider      = Slider(0, 0, pw, "Jog Steps",       1,   200,  10,   step=1,   fmt="{}")
    backlash_slider = Slider(0, 0, pw, "Backlash Steps",  0,   100,  10,   step=1,   fmt="{}")
    min_dist_slider = Slider(0, 0, pw, "Min Dist (cm)",   0,   150,  15,   step=1,   fmt="{:.0f}cm")

    all_sliders   = [range_slider, speed_slider, jog_slider, backlash_slider, min_dist_slider]
    prev_speed    = speed_slider.value
    prev_jog      = jog_slider.value
    prev_backlash = backlash_slider.value
    dragging      = None

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                send_cmd("Q"); time.sleep(0.5); running = False
            if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                send_cmd("Q"); time.sleep(0.5); running = False

            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                pos = event.pos
                if btn_enable.clicked(pos):
                    btn_enable.active = True;  btn_disable.active = False; send_cmd("E")
                elif btn_disable.clicked(pos):
                    btn_enable.active = False; btn_disable.active = True;  send_cmd("D")
                elif btn_auto.clicked(pos):
                    btn_auto.active = True;    btn_manual.active = False;  send_cmd("A")
                elif btn_manual.clicked(pos):
                    btn_auto.active = False;   btn_manual.active = True;   send_cmd("M")
                elif btn_left.clicked(pos):    send_cmd("L")
                elif btn_right.clicked(pos):   send_cmd("R")
                elif btn_home.clicked(pos):    send_cmd("H")
                elif btn_stop.clicked(pos):
                    send_cmd("Q"); time.sleep(0.5); running = False
                else:
                    for s in all_sliders:
                        if s.hit_test(pos):
                            dragging = s; s.update(pos[0]); break

            if event.type == pygame.MOUSEBUTTONUP   and event.button == 1: dragging = None
            if event.type == pygame.MOUSEMOTION     and dragging: dragging.update(event.pos[0])

        if speed_slider.value != prev_speed:
            prev_speed = speed_slider.value; send_cmd("S" + str(int(speed_slider.value)))
        if jog_slider.value != prev_jog:
            prev_jog = jog_slider.value; send_cmd("J" + str(int(jog_slider.value)))
        if backlash_slider.value != prev_backlash:
            prev_backlash = backlash_slider.value; send_cmd("B" + str(int(backlash_slider.value)))

        range_m  = range_slider.value
        min_dist = min_dist_slider.value

        screen.fill((10, 10, 15))

        # 1m  rings
        for i in range(1, max(1, math.ceil(range_m)) + 1):
            ring_m = min(float(i), range_m)
            ring_r = int((CENTER - 40) * ring_m / range_m)
            pygame.draw.circle(screen, (30, 40, 30), (CENTER, CENTER), ring_r, 1)
            screen.blit(font_sm.render(str(int(ring_m)) + "m", True, (30, 40, 30)), (CENTER + ring_r + 3, CENTER - 8))

        pygame.draw.line(screen, (40, 55, 40), (40, CENTER), (RADAR_SIZE - 40, CENTER), 1)
        pygame.draw.line(screen, (40, 55, 40), (CENTER, 40), (CENTER, RADAR_SIZE - 40), 1)

        for deg in range(0, 360, 30):
            rad = math.radians(deg)
            r   = CENTER - 25
            screen.blit(font_sm.render(str(deg), True, (30, 40, 30)),
                        (CENTER + int(r * math.cos(rad)) - 8, CENTER - int(r * math.sin(rad)) - 6))

        arm = math.radians(current_angle)
        arm_len = CENTER - 40
        pygame.draw.line(screen, (0, 100, 0), (CENTER, CENTER),
                         (CENTER + int(arm_len * math.cos(arm)), CENTER - int(arm_len * math.sin(arm))), 1)

        with points_lock:
            old = []
            visible = []
            for bucket, (angle, dist, strength, sid) in points.items():
                age = sweep_id - sid
                if age > 3:
                    old.append(bucket); continue
                if dist < min_dist: continue
                sx, sy = polar_to_screen(angle, dist, range_m)
                if (sx - CENTER) ** 2 + (sy - CENTER) ** 2 > (CENTER - 40) ** 2: continue
                fade  = [1.0, 0.75, 0.5, 0.25][min(age, 3)]
                base  = strength_to_color(strength)
                visible.append((bucket, sx, sy, fade, base))
            for b in old:
                del points[b]

        if len(visible) > 1:
            visible.sort(key=lambda p: p[0])
            for i in range(len(visible) - 1):
                b1, sx1, sy1, f1, c1 = visible[i]
                b2, sx2, sy2, f2, c2 = visible[i + 1]
                if b2 - b1 > 6: continue
                f = (f1 + f2) / 2
                color = (int(((c1[0]+c2[0])//2)*f), int(((c1[1]+c2[1])//2)*f), int(((c1[2]+c2[2])//2)*f))
                pygame.draw.line(screen, color, (sx1, sy1), (sx2, sy2), 3)

        pygame.draw.circle(screen, (0, 255, 0), (CENTER, CENTER), 4)

        screen.blit(font_lg.render("360 LiDAR", True, (0, 220, 100)), (10, 8))
        screen.blit(font_med.render(ble_status, True, (0, 200, 0)), (10, 34))
        screen.blit(font_sm.render("Angle: " + str(round(current_angle, 1)), True, (0, 200, 0)), (10, 60))

        pygame.draw.rect(screen, (18, 18, 22), (RADAR_SIZE, 0, PANEL_W, WIN_H))
        pygame.draw.line(screen, (40, 50, 40), (RADAR_SIZE, 0), (RADAR_SIZE, WIN_H), 1)

        mouse_pos = pygame.mouse.get_pos()
        y = 10
        sh = 44  # slider block height

        def row2(b1, b2):
            b1.rect.x = px;          b1.rect.y = y
            b2.rect.x = px + bw2 + 8; b2.rect.y = y
            b1.draw(screen, font_med, mouse_pos)
            b2.draw(screen, font_med, mouse_pos)

        def rowf(b):
            b.rect.x = px; b.rect.y = y
            b.draw(screen, font_med, mouse_pos)

        def rows(s):
            s.x = px; s.y = y + 20
            s.draw(screen, font_sm)

        row2(btn_enable, btn_disable); y += bh + 8
        row2(btn_auto,   btn_manual);  y += bh + 8
        rows(range_slider);            y += sh
        row2(btn_left, btn_right);     y += bh + 8
        rowf(btn_home);                y += bh + 8
        rows(speed_slider);            y += sh
        rows(jog_slider);              y += sh
        rows(backlash_slider);         y += sh
        rows(min_dist_slider);         y += sh
        rowf(btn_stop)

        pygame.display.flip()
        clock.tick(FPS)

    pygame.quit()
    if ble_client is not None:
        try:
            loop = asyncio.new_event_loop()
            loop.run_until_complete(ble_client.disconnect())
            loop.close()
        except Exception:
            pass

if __name__ == "__main__":
    main()