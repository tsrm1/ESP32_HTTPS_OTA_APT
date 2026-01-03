#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style>
      body {
        font-family: sans-serif;
        margin: 0;
        background: #f0f2f5;
        color: #1c1e21;
      }
      .nav {
        display: flex;
        background: #fff;
        position: sticky;
        top: 0;
        z-index: 100;
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
      }
      .nav-tab {
        flex: 1;
        padding: 15px;
        text-align: center;
        cursor: pointer;
        border-bottom: 3px solid transparent;
      }
      .nav-tab.active {
        border-bottom: 3px solid #007bff;
        color: #007bff;
        font-weight: bold;
      }
      .container {
        padding: 10px;
        max-width: 600px;
        margin: auto;
      }
      .page {
        display: none;
      }
      .page.active {
        display: block;
      }
      .card-grid {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 8px;
        margin-bottom: 15px;
      }
      .card {
        background: #fff;
        padding: 15px;
        border-radius: 12px;
        text-align: center;
        box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
      }
      .card h4 {
        margin: 0 0 5px 0;
        font-size: 13px;
        color: #666;
        text-transform: uppercase;
      }
      .card div {
        font-size: 22px;
        font-weight: bold;
        color: #007bff;
      }
      .section {
        background: #f0f2f5;
        padding: 15px;
        border-radius: 12px;
        margin: 12px;
        display: none;
        border: 2px solid #28a745;
      }
      .section h3 {
        color: #28a745;
        margin-top: 0;
        font-size: 16px;
      }
      .row {
        display: flex;
        align-items: center;
        margin-bottom: 10px;
        position: relative;
      }
      .row label {
        width: 110px;
        font-size: 13px;
        font-weight: 600;
      }
      .row input,
      textarea {
        flex: 1;
        padding: 8px;
        border: 1px solid #ddd;
        border-radius: 6px;
        background: #f3f3f3;
      }
      .btn {
        background: #28a745;
        color: #fff;
        border: none;
        padding: 12px;
        border-radius: 8px;
        width: 100%;
        cursor: pointer;
        font-size: 15px;
        font-weight: bold;
      }
      .btn-add {
        background: #007bff;
      }
      .pass-wrapper {
        position: relative;
        flex: 1;
        display: flex;
        align-items: center;
      }
      .pass-wrapper input {
        width: 100%;
        padding: 8px 35px 8px 8px;
        border: 1px solid #ddd;
        border-radius: 6px;
      }
      .toggle-eye {
        position: absolute;
        right: 10px;
        cursor: pointer;
        color: #999;
        font-size: 18px;
        user-select: none;
      }
      .switch {
        position: relative;
        display: inline-block;
        width: 44px;
        height: 24px;
        vertical-align: middle;
      }
      .switch input {
        opacity: 0;
        width: 0;
        height: 0;
      }
      .slider {
        position: absolute;
        cursor: pointer;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background-color: #ccc;
        transition: 0.3s;
        border-radius: 24px;
      }
      .slider:before {
        position: absolute;
        content: "";
        height: 18px;
        width: 18px;
        left: 3px;
        bottom: 3px;
        background-color: white;
        transition: 0.3s;
        border-radius: 50%;
      }
      input:checked + .slider {
        background-color: #28a745;
      }
      input:checked + .slider:before {
        transform: translateX(20px);
      }
      .net-item {
        display: flex;
        justify-content: space-between;
        padding: 12px;
        border-bottom: 1px solid #eee;
        cursor: pointer;
        font-size: 14px;
      }
      .net-item:hover {
        background: #f8f9fa;
      }
      /* Для DS18B20 */
      /* .details {
        background: #fff;
        border-radius: 8px;
        margin-bottom: 6px;
        border: 1px solid #eee;
        overflow: hidden;
      } */
      /* summary {
        padding: 14px;
        cursor: pointer;
        display: flex;
        align-items: center;
        font-weight: 500;
        outline: none;
      } */
      /* details-content {
        padding: 15px;
        border-top: 1px solid #f0f0f0;
        background: #fafafa;
      } */
      .acc-item {
        background: #fff;
        border-radius: 8px;
        margin-bottom: 8px;
        overflow: hidden;
        border: 1px solid #eee;
      }
      .acc-header {
        padding: 12px 15px;
        display: flex;
        align-items: center;
        cursor: pointer;
        background: #fff;
        justify-content: space-between;
        font-size: 1.2rem;
        font-weight: bold;
      }
      .acc-panel {
        display: none;
        padding: 15px;
        border-top: 1px solid #eee;
        background: #fafafa;
      }
      .acc-panel.show {
        display: block;
      }
    </style>
  </head>
  <body>
    <div class="nav">
      <div class="nav-tab active" onclick="openTab(event, 'tab-mon')">
        Monitor
      </div>
      <div class="nav-tab" onclick="openTab(event, 'tab-sens')">Sensors</div>
      <div class="nav-tab" onclick="openTab(event, 'tab-srv')">Services</div>
    </div>
    <div class="container">
      <div id="tab-mon" class="page active">
        <div class="card-grid" id="cards-cont"></div>
      </div>

      <div id="tab-sens" class="page">
        <div class="acc-cont">
          <!-- Sensor BME280 -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)"
                >BME280 (Temperature, Humidity, Pressure)</span
              >
              <label class="switch"
                ><input
                  type="checkbox"
                  id="bme_en"
                  onclick="setSwitcher('bme_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_bme"></div>
          </div>

          <!-- Sensor DHT22 -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)"
                >DHT22 (Temperature, Humadity)</span
              >
              <label class="switch"
                ><input
                  type="checkbox"
                  id="dht_en"
                  onclick="setSwitcher('dht_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_dht"></div>
          </div>

          <!-- Sensor DS18B20 -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">DS18B20 (Temperature)</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="ds_en"
                  onclick="setSwitcher('ds_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_ds"></div>
          </div>

          <!-- Sensor TCRT5000 -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">TCRT5000 (Light)</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="tcrt_en"
                  onclick="setSwitcher('tcrt_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_tcrt"></div>
          </div>

          <!-- Sensor PIR -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">PIR (Motion)</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="pir_en"
                  onclick="setSwitcher('pir_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_pir"></div>
          </div>

          <!-- Sensor LD2420 -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">LD2420 (Presence)</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="ld_en"
                  onclick="setSwitcher('ld_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_ld"></div>
          </div>

          <!-- Sensor DOOR -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">Magnetic sensor (Door)</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="dr_en"
                  onclick="setSwitcher('dr_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_dr"></div>
          </div>

          <!-- Sensor FLOOD -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">Flood sensor (Flooding)</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="fl_en"
                  onclick="setSwitcher('fl_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_fl"></div>
          </div>

          <!-- Sensor Resistor 5516 -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">Resistor 5516 (Light)</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="lr_en"
                  onclick="setSwitcher('lr_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_lr"></div>
          </div>

          <!-- Sensor RELE -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">RELEYx4 (Relay unit)</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="r_en"
                  onclick="setSwitcher('r_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_r"></div>
          </div>
        </div>
      </div>

      <div id="tab-srv" class="page">
        <div class="acc-cont">
          <!-- Service WiFi -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">WiFi</span>
              <label class="switch">
                <input
                  type="checkbox"
                  id="w_en"
                  onclick="setSwitcher('w_en')"
                />
                <span class="slider"></span>
              </label>
            </div>
            <div class="acc-panel" id="cont_w">
              <button class="btn btn-add" onclick="scanWiFi()">
                Scan Wi-Fi
              </button>
              <div id="nets"></div>
              <div class="row" style="margin-top: 15px">
                <label>SSID</label><input id="ssid" readonly />
              </div>
              <div class="row">
                <label>Password</label>
                <div class="pass-wrapper">
                  <input id="w_pass" type="password" /><span
                    class="toggle-eye"
                    onclick="togglePass('w_pass')"
                    >👁️</span
                  >
                </div>
              </div>
              <button class="btn" onclick="connWiFi()">Connect</button>

              <div id="cur-wifi" class="section">
                <h3>Connected to:</h3>
                <div class="row">
                  <label>SSID</label
                  ><span id="cur_ssid" style="font-weight: bold"></span>
                </div>
                <div class="row">
                  <label>IP</label
                  ><span id="cur_ip" style="font-weight: bold"></span>
                </div>
                <div class="row">
                  <label>RSSI</label><span id="cur_rssi"></span>
                </div>
                <div class="row">
                  <label>Link</label
                  ><a
                    id="cur_link"
                    href=""
                    target="_blank"
                    style="
                      color: #007bff;
                      font-weight: bold;
                      word-break: break-all;
                    "
                  ></a>
                </div>
              </div>
            </div>
          </div>
        </div>
        <div class="acc-cont">
          <!-- Service Telegram -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">Telegram</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="tg_en"
                  onclick="setSwitcher('tg_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_tg">
              <div class="row"><label>Bot Token</label><input id="tg_t" /></div>
              <div id="ids-container"></div>
              <button
                class="btn btn-add"
                style="margin-bottom: 12px"
                onclick="addUserId()"
              >
                Add UserID
              </button>
              <button class="btn" onclick="connTG()">Connect</button>
            </div>
          </div>
        </div>
        <div class="acc-cont">
          <!-- Service MQTT -->
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">MQTT</span>
              <label class="switch"
                ><input
                  type="checkbox"
                  id="m_en"
                  onclick="setSwitcher('m_en')" /><span class="slider"></span
              ></label>
            </div>
            <div class="acc-panel" id="cont_m">
              <div class="row"><label>Broker IP</label><input id="m_ip" /></div>
              <div class="row">
                <label>Port</label><input id="m_port" type="number" />
              </div>
              <div class="row"><label>User</label><input id="m_u" /></div>
              <div class="row">
                <label>Pass</label>
                <div class="pass-wrapper">
                  <input id="m_p" type="password" /><span
                    class="toggle-eye"
                    onclick="togglePass('m_p')"
                    >👁️</span
                  >
                </div>
              </div>
              <div class="row"><label>Topic</label><input id="m_bt" /></div>
              <div class="row">
                <label>Interval (s)</label><input id="m_i" type="number" />
              </div>
              <button class="btn" onclick="connMQTT()">Connect</button>
            </div>
          </div>
        </div>

        <div class="acc-cont">
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">Web Security</span>
            </div>
            <div class="acc-panel" id="cont_ws">
              <div class="row"><label>Old login</label><input id="w_u" /></div>
              <div class="row">
                <label>Old password</label>
                <div class="pass-wrapper">
                  <input id="w_p" type="password" /><span
                    class="toggle-eye"
                    onclick="togglePass('w_p')"
                    >👁️</span
                  >
                </div>
              </div>
              <div class="row">
                <label>New login</label><input id="w_u_n" />
              </div>
              <div class="row">
                <label>New password</label>
                <div class="pass-wrapper">
                  <input id="w_p_n" type="password" /><span
                    class="toggle-eye"
                    onclick="togglePass('w_p_n')"
                    >👁️</span
                  >
                </div>
              </div>
              <button class="btn" onclick="setWS()">Save</button>
            </div>
          </div>
        </div>

        <div class="acc-cont">
          <div class="acc-item">
            <div class="acc-header">
              <span onclick="toggleAcc(this)">Update OS</span>
            </div>
            <div class="acc-panel" id="cont_upd">
              <button
                class="btn btn-add"
                style="margin-bottom: 12px"
                onclick="checkUpdateOS()"
              >
                Check update
              </button>

              <div class="section" id="s_ver_info">
                <div class="row">
                  <label>Current ver.</label><input id="s_cur_ver" readonly />
                </div>
                <div class="row">
                  <label>Server ver.</label><input id="s_serv_ver" readonly />
                </div>
                <div class="row">
                  <label for="s_serv_notes">Description of changes:</label>
                  <textarea
                    id="s_serv_notes"
                    name="story"
                    rows="6"
                    cols="70"
                    readonly
                  ></textarea>
                </div>
              </div>

              <button class="btn" onclick="updateOS()">Update OS</button>
            </div>
          </div>
        </div>
      </div>
    </div>
    <!-- ************************************************************************************************** -->
    <script>
      const BASE_URL = "";
      const SENS_MAP = [
        // key, num of pins, num of values
        ["bme", 1, 3],
        ["dht", 1, 2],
        ["ds", 1, 4],
        ["tcrt", 1, 1],
        ["pir", 1, 1],
        ["ld", 1, 1],
        ["dr", 1, 1],
        ["fl", 1, 1],
        ["lr", 1, 1],
        ["r", 4, 4],
      ];
      const switchers = [
        "w_en",
        "tg_en",
        "m_en",
        "bme_en",
        "dht_en",
        "ds_en",
        "tcrt_en",
        "pir_en",
        "ld_en",
        "dr_en",
        "fl_en",
        "lr_en",
        "r_en",
      ];
      let activeIds = 0;

      // async function scanDS() {
      //   const pin = document.getElementById("sen_g6").value;
      //   const btn = document.querySelector(".btn-search");
      //   btn.innerText = "Поиск...";
      //   try {
      //     const res = await fetch(`${BASE_URL}/api/ds-scan?pin=${pin}`);
      //     const found = await res.json();
      //     const listDiv = document.getElementById("ds-found-list");
      //     listDiv.innerHTML = "";
      //     found.forEach((item, i) => {
      //       listDiv.innerHTML += `
      //             <div class="ds-item" data-mac="${item.m}">
      //                 <span>№${i + 1} (${item.m}) <span class="ds-temp">[ ${
      //         item.t
      //       } °C ]</span></span>
      //                 <input type="number" class="ds-u-idx" value="${i + 1}">
      //             </div>`;
      //     });
      //   } catch (e) {
      //     alert("Scanning error");
      //   } finally {
      //     btn.innerText = "Scan";
      //   }
      // }

      // function buildSensorsMenu() {
      //   const acc = document.getElementById("sensor-accordion");
      //   acc.innerHTML = "";

      //   Object.entries(SENSOR_MAP).forEach(([id, s]) => {
      //     const details = document.createElement("details");

      //     const summary = document.createElement("summary");
      //     summary.innerHTML = `
      //     <input type="checkbox" onchange="toggleSensor('${id}', this.checked)">
      //     <span style="margin-left:8px">${s.title}</span>
      //   `;

      //     const content = document.createElement("div");
      //     content.className = "details-content";

      //     // GPIO inputs
      //     for (let i = 0; i < s.gpio; i++) {
      //       content.innerHTML += `
      //       <div class="row">
      //         <label>GPIO ${i + 1}</label>
      //         <input type="number" min="0" max="39" id="${id}_gpio_${i}">
      //       </div>
      //     `;
      //     }

      //     // DS18B20 special block
      //     if (s.onewire) {
      //       content.innerHTML += `
      //       <button class="btn" onclick="scanDS('${id}')">Scan</button>
      //       <div id="${id}_list"></div>
      //     `;
      //     }

      //     details.appendChild(summary);
      //     details.appendChild(content);
      //     acc.appendChild(details);
      //   });
      // }

      // function toggleSensor(sensorId, state) {
      //   SENSOR_MAP[sensorId].cards.forEach((cardId) => {
      //     const el = document.getElementById(cardId);
      //     if (el) el.style.display = state ? "block" : "none";
      //   });
      // }

      // Переключение закладок
      function openTab(e, n) {
        document
          .querySelectorAll(".page")
          .forEach((p) => p.classList.remove("active"));
        document
          .querySelectorAll(".nav-tab")
          .forEach((t) => t.classList.remove("active"));
        document.getElementById(n).classList.add("active");
        e.currentTarget.classList.add("active");
        getSettings(); // запрос на обновление настроек
        pollWifiStatus(); // запрос состояния подключения к локальной сети WiFi
      }

      // Скрытие/отображение содержимого закладок
      function toggleSets() {
        const p = document.getElementById("settings-panel");
        p.style.display = p.style.display === "block" ? "none" : "block";
      }

      // Скрытие/отображение содержимого меню (аккардеон)
      function toggleAcc(el) {
        el.parentElement.nextElementSibling.classList.toggle("show");
      }

      // /Переключение видимости пароля
      function togglePass(id) {
        const el = document.getElementById(id);
        el.type = el.type === "password" ? "text" : "password";
      }

      // Добавление нового UserID для Telegram
      function addUserId(val = "") {
        if (activeIds >= 6) return;
        const container = document.getElementById("ids-container");
        const div = document.createElement("div");
        div.className = "row";
        div.innerHTML = `<label>UserID ${
          activeIds + 1
        }</label><input id="id${activeIds}" value="${val}" style="flex:1; padding:8px; border:1px solid #ddd; border-radius:6px;">`;
        container.appendChild(div);
        activeIds++;
      }
      // const maps = [
      //   ["card-dht-t", "card-dht-h"],
      //   ["card-lux-5516"],
      //   ["card-tcrt"],
      //   ["card-pir"],
      //   ["card-pres"],
      //   ["card-bme-t", "card-bme-h", "card-bme-p"],
      //   ["card-t1", "card-t2", "card-t3", "card-t4"],
      //   ["card-door"],
      //   ["card-flood"],
      // ];
      // function applyVis(senArray, relEn) {
      //   senArray.forEach((en, i) => {
      //     if (maps[i])
      //       maps[i].forEach((id) => {
      //         const el = document.getElementById(id);
      //         if (el) el.style.display = en ? "block" : "none";
      //       });
      //   });
      //   for (let i = 0; i < 4; i++) {
      //     const el = document.getElementById("card-r" + i);
      //     if (el) el.style.display = relEn ? "block" : "none";
      //   }
      // }

      // Отображение доступных сетей WiFi
      function scanWiFi() {
        const n = document.getElementById("nets");
        n.innerHTML = '<div style="padding:10px">🔎 Scanning...</div>';
        fetch(`${BASE_URL}/api/wifi-scan`)
          .then((r) => r.json())
          .then((d) => {
            n.innerHTML = "";
            d.sort((a, b) => b.rssi - a.rssi);
            d.forEach((x) => {
              const i = document.createElement("div");
              i.className = "net-item";
              let sig = x.rssi > -60 ? "🟢" : x.rssi > -75 ? "🟡" : "🔴";
              let lock = x.enc ? "🔒" : "🔓";
              i.innerHTML = `<span>${sig} ${lock} <b>${x.ssid}</b></span> <span style="color:#888">${x.rssi} dBm</span>`;
              i.onclick = () => {
                document.getElementById("ssid").value = x.ssid;
              };
              n.appendChild(i);
            });
          })
          .catch((e) => {
            n.innerHTML =
              '<div style="padding:10px; color:red">Scanning error</div>';
          });
      }

      // Получение состояния подключения к локальной сети WiFi
      function pollWifiStatus() {
        fetch(`${BASE_URL}/api/wifi-status`)
          .then((r) => r.json())
          .then((d) => {
            if (d.connected) {
              document.getElementById("cur-wifi").style.display = "block";
              document.getElementById("cur_ssid").innerText = d.ssid;
              document.getElementById("cur_ip").innerText = d.ip;
              document.getElementById("cur_rssi").innerText = d.rssi + " dBm";
              const url = "http://" + d.ip + "/";
              const link = document.getElementById("cur_link");
              link.href = url;
              link.innerText = url;
              fetch(`${BASE_URL}/api/ap-disable`);
            } else {
              setTimeout(pollWifiStatus, 3000);
            }
          });
      }
      // Отправка log/pass для изменения доступа к Homepage
      function setWS() {
        const msg = {};
        msg["w_u"] = document.getElementById("w_u").value;
        msg["w_p"] = document.getElementById("w_p").value;
        msg["w_u_n"] = document.getElementById("w_u_n").value;
        msg["w_p_n"] = document.getElementById("w_p_n").value;
        fetch(`${BASE_URL}/api/set-ws`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          //mode: "cors",
          body: JSON.stringify(msg),
        })
          .then(() => {
            document.getElementById("w_u").value = "";
            document.getElementById("w_p").value = "";
            document.getElementById("w_u_n").value = "";
            document.getElementById("w_p_n").value = "";
            alert("Admin Login&Password was changed.");
          })
          .catch((error) => {
            alert(error);
          });
      }

      //     document.getElementById("m_ip").value = c.m_ip;
      //     document.getElementById("m_port").value = c.m_port;
      //     document.getElementById("m_t").value = c.m_t;
      //     document.getElementById("m_i").value = c.m_i;

      //     for (let i = 0; i < c.ids_c; i++) addUserId(c.ids[i]);
      //     if (activeIds === 0) addUserId("");
      //     // for (let i = 0; i < 9; i++) {
      //     //   if (document.getElementById("ch" + i))
      //     //     document.getElementById("ch" + i).checked = c.sen[i];
      //     // }
      //     //applyVis(c.sen, c.r_en);
      //     pollWifiStatus();
      //   });

      // Запуск периодического запроса состояния РЕЛЕ, период 1 раз в 1 секунду
      setInterval(() => {
        fetch(`${BASE_URL}/api/get-relays`)
          .then((r) => r.json())
          .then((d) => {
            d.r.forEach(
              (v, i) => (document.getElementById("r" + i).checked = v)
            );
          })
          .catch((error) => {
            console.error(error);
          });
      }, 1000);

      // Запуск периодического запроса значений с датчиков, период 1 раз в 5 секунд
      setInterval(() => {
        fetch(`${BASE_URL}/api/values`)
          .then((r) => r.json())
          .then((v) => {
            [
              "bme_v0",
              "bme_v1",
              "bme_v2",
              "dht_v0",
              "dht_v1",
              "ds_v0",
              "ds_v1",
              "ds_v2",
              "ds_v3",
              "tcrt_v0",
              "lr_v0",
            ].forEach((f) => {
              if (document.getElementById("card_" + f))
                document.getElementById("card_" + f).innerText = v[f];
            });
            const updateStatus = (id, val, tT, tF) => {
              const el = document.getElementById(id);
              if (!el) return;
              el.innerText = val ? tT : tF;
              el.style.color = val ? "red" : "#007bff";
            };
            updateStatus("card_pir_v0", v.pir_v, "ЕСТЬ", "НЕТ");
            updateStatus("card_ld_v0", v.ld_v, "ЕСТЬ", "НЕТ");
            updateStatus("card_dr_v0", v.dr_v, "ОТКР", "ЗАКР");
            updateStatus("card_fl_v0", v.fl_v, "ЕСТЬ", "НЕТ");
          })
          .catch((error) => {
            console.error(error);
          });
      }, 5000);

      // Отправка запроса на переключение реле
      function setRelay(idx) {
        const st = document.getElementById("r" + idx).checked;
        fetch(`${BASE_URL}/api/set-relay?id=${idx}&st=${st}`);
      }

      // Отправка запроса на переключение переключателя
      function setSwitcher(idx) {
        const f = new FormData();
        f.append(idx, document.getElementById(idx).checked);
        fetch(`${BASE_URL}/api/set-srv`, { method: "POST", body: f });
      }

      // Отправка SSID/PASS для подключения к локальной сети WiFi
      function connWiFi() {
        const f = new FormData();
        f.append("s", document.getElementById("ssid").value);
        f.append("p", document.getElementById("w_pass").value);
        fetch(`${BASE_URL}/api/wifi-connect`, { method: "POST", body: f }).then(
          () => {
            alert("Connecting to WiFi...");
            pollWifiStatus();
          }
        );
      }

      document.addEventListener("DOMContentLoaded", () => {
        renderSensorsInfo();
        showCurrentOS();
        getSettings();
      });

      function showCurrentOS() {
        document.getElementById(`s_cur_ver`).value = "0.2.7";
      }

      function renderSensorsInfo() {
        function generateSensorCards() {
          let html = "";
          for (let i = 0; i < SENS_MAP.length - 1; i++)
            for (let j = 0; j < SENS_MAP[i][2]; j++)
              html += `<div class="card" id="card_${SENS_MAP[i][0]}${j}"><h4 id="card_${SENS_MAP[i][0]}_l${j}"></h4><div id="card_${SENS_MAP[i][0]}_v${j}">--</div></div>`;
          for (let i = 0; i < 4; i++)
            html += `<div class="card" id="card-r${i}"><h4 id="card_r_l${i}"></h4><label class="switch"><input type="checkbox" id="r${i}" onchange="setRelay(${i})" /><span class="slider"></span></label></div>`;
          document.getElementById(`cards-cont`).innerHTML = html;
          html = "";
        }
        function generateSensorSettings() {
          SENS_MAP.forEach((sensData) => {
            let html = "";
            // Вывод GPIO
            for (let j = 0; j < sensData[1]; j++)
              html += `<div class="row"><label>GPIO${
                sensData[1] === 1 ? "" : " " + (j + 1)
              }:</label><input type="text" id="${sensData[0]}_p${j}"/></div>`;
            // Вывод Label
            for (let j = 0; j < sensData[2]; j++)
              html += `<div class="row"><label>Label${
                sensData[2] === 1 ? "" : " " + (j + 1)
              }:</label><input type="text" id="${sensData[0]}_l${j}"/></div>`;
            // Вывод Topic
            for (let j = 0; j < sensData[2]; j++)
              html += `<div class="row"><label>Topic${
                sensData[2] === 1 ? "" : " " + (j + 1)
              }:</label><input type="text" id="${sensData[0]}_t${j}"/></div>`;

            html += `<button class="btn" onclick="applySet('${sensData[0]}')">Apply</button>`;

            document.getElementById(`cont_${sensData[0]}`).innerHTML = html;
          });
        }
        generateSensorCards();
        generateSensorSettings();
      }

      function getSettings() {
        fetch(`${BASE_URL}/api/get-all`)
          .then((response) => {
            if (!response.ok) throw new Error("Error! No response.");
            return response.json();
          })
          .then((data) => {
            // if (data.connected)
            const values = ["m_ip", "m_port", "m_bt", "m_i"];

            switchers.forEach((key) => {
              const element = document.getElementById(key);
              if (element) element.checked = data[key];
            });

            values.forEach((key) => {
              const el = document.getElementById(key);
              if (el) el.value = data[key];
              const el_card = document.getElementById(`card_${key}`);
              if (el_card) el_card.textContent = data[key];
            });

            SENS_MAP.forEach((sensData) => {
              for (let i = 0; i < sensData[1]; i++) {
                // Sensors, Pin
                const pin = document.getElementById(`${sensData[0]}_p${i}`);
                if (pin) pin.value = data[`${sensData[0]}_p`][i];
              }
              for (let i = 0; i < sensData[2]; i++) {
                // Monitor, Title
                const title = document.getElementById(
                  `card_${sensData[0]}_l${i}`
                );
                if (title) title.textContent = data[`${sensData[0]}_l`][i];
                // Sensors, Label
                const label = document.getElementById(`${sensData[0]}_l${i}`);
                if (label) label.value = data[`${sensData[0]}_l`][i];
                // Sensors, Topic
                const topic = document.getElementById(`${sensData[0]}_t${i}`);
                if (topic) topic.value = data[`${sensData[0]}_t`][i];
              }
            });

            // for (let i = 0; i < 4; i++) {
            //   const element = document.getElementById(`r_p${i}`);
            //   if (element) element.value = data.r_p[i];
            // }

            // document.getElementById("m_ip").value = data[m_ip];
            // document.getElementById("m_port").value = data[m_port];
            // document.getElementById("m_t").value = data[m_t];
            // document.getElementById("m_i").value = data[m_i];

            // for (let i = 0; i < data["ids_c"]; i++) addUserId(data.ids[i]);
            // if (activeIds === 0) addUserId("");

            // } else setTimeout(getSettings, 1000);
          })
          .catch((error) => {
            console.error(error);
          });
      }
      function applySet(key) {
        //console.log(`ApplaySet(${key})`);
        for (let i = 0; i < SENS_MAP.length; i++) {
          if (SENS_MAP[i][0] === key) {
            const msg = { pins: [], labels: [], topics: [] };
            for (let j = 0; j < SENS_MAP[i][2]; j++) {
              msg.labels.push(
                document.getElementById(`${SENS_MAP[i][0]}_l${j}`).value
              );
              msg.topics.push(
                document.getElementById(`${SENS_MAP[i][0]}_t${j}`).value
              );
              msg.pins.push(
                Number(document.getElementById(`${SENS_MAP[i][0]}_p${j}`).value)
              );
            }
            fetch(`${BASE_URL}/api/set-sens`, {
              method: "POST",
              body: JSON.stringify(msg),
            }).then(() => alert("Saved."));
          }
        }
      }

      // function checkUpdateOS() {
      //   // fetch(`${BASE_URL}/api/get-remote-manifest`, {
      //   fetch(
      //     `https://secobj.netlify.app/esp32/ESP32_HTTPS_OTA_APT/manifest.json`,

      function checkUpdateOS() {
        const MANIFEST_URL =
          "https://secobj.netlify.app/esp32/ESP32_HTTPS_OTA_APT/manifest.json";
        fetch(MANIFEST_URL, {
          method: "GET",
          headers: { Accept: "application/json" },
        })
          .then((response) => {
            if (!response.ok)
              throw new Error(`Ошибка HTTP: ${response.status}`);
            return response.json();
          })
          .then((data) => {
            document.getElementById("s_ver_info").style.display = "block";
            document.getElementById("s_serv_ver").value = data.version;
            document.getElementById("s_serv_notes").textContent = data.notes;
            return data;
          })
          .catch((error) => {
            console.error("Не удалось получить манифест:", error);
          });
      }
      function updateOS() {
        const f = new FormData();
        f.append("up-to-date", true);
        fetch(`${BASE_URL}/api/update`, { method: "POST", body: f }).then(
          (response) => {
            alert("Checked for new version and update.");
          }
        );
      }
      function applySet(type) {
        var formData = new FormData();
        var inputs = document.querySelectorAll('[id^="' + type + '_"]');
        inputs.forEach(function (input) {
          if (input.type === "checkbox")
            formData.append(input.id, input.checked ? "true" : "false");
          else formData.append(input.id, input.value);
        });
        fetch(BASE_URL + "/api/set-sens", {
          method: "POST",
          body: formData,
        })
          .then(function (response) {
            if (response.ok) {
              alert("Settings " + type.toUpperCase() + " saved.");
            } else {
              alert("Server ERROR: " + response.status);
            }
          })
          .catch(function (error) {
            console.error("Connection ERROR:", error);
            alert("Connection with ESP32 ERROR!");
          });
      }

      function saveSens() {
        const f = new FormData();
        f.append("bme_en", document.getElementById("bme_en").checked);
        f.append("dht_en", document.getElementById("dht_en").checked);
        f.append("ds_en", document.getElementById("ds_en").checked);
        f.append("tcrt_en", document.getElementById("tcrt_en").checked);
        f.append("pir_en", document.getElementById("pir_en").checked);
        f.append("ld_en", document.getElementById("ld_en").checked);
        f.append("dr_en", document.getElementById("dr_en").checked);
        f.append("fl_en", document.getElementById("fl_en").checked);
        f.append("lr_en", document.getElementById("lr_en").checked);
        f.append("r_en", document.getElementById("r_en").checked);
        fetch(`${BASE_URL}/api/set-sens`, { method: "POST", body: f }).then(
          () => alert("Saved.")
        );
      }
      function saveSrv() {
        const f = new FormData();
        f.append("w_en", document.getElementById("w_en").checked);
        f.append("tg_en", document.getElementById("tg_en").checked);
        f.append("tg_t", document.getElementById("tg_t").value);
        f.append("tg_ids", activeIds);
        for (let i = 0; i < activeIds; i++)
          f.append("id" + i, document.getElementById("id" + i).value);
        f.append("m_en", document.getElementById("m_en").checked);
        f.append("m_ip", document.getElementById("m_ip").value);
        f.append("m_port", document.getElementById("m_port").value);
        f.append("m_u", document.getElementById("m_u").value);
        f.append("m_p", document.getElementById("m_p").value);
        f.append("m_bt", document.getElementById("m_t").value);
        f.append("m_i", document.getElementById("m_i").value);
        fetch(`${BASE_URL}/api/set-srv`, { method: "POST", body: f }).then(() =>
          alert("Saved.")
        );
      }
    </script>
  </body>
</html>

)rawliteral";

#endif