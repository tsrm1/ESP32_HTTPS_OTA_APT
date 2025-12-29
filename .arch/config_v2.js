const CONFIG = {
  // Общие настройки системы
  system: {
    device_name: "Smart-Controller-01",
    reboot_interval: 0,
  },

  services: {
    wifi: {
      ssid: "",
      pass: "",
      ap_mode: true, // Точка доступа, если основной WiFi недоступен
    },
    web: {
      user: "",
      pass: "",
    },
    telegram: {
      enabled: false,
      token: "",
      ids_count:0,
      ids: [],
    },
    mqtt: {
      enabled: false,
      host: "",
      port: 1883,
      user: "",
      pass: "",
      base_topic: "home/sensor1", // Префикс для всех топиков
      interval: 5, // Интервал отправки в секундах
    },
  },

  // Группа датчиков и исполнительных устройств
  nodes: {
    // Климатические датчики (I2C/OneWire/Bus)
    climate: {
      bme280: {
        enabled: true,
        type: "I2C",
        labels: ["Температура", "Влажность", "Давление"],
        units: ["°C", "%", "Pa"],
        ui_cards: ["card-bme-t", "card-bme-h", "card-bme-p"],
        topics: ["/bme-t", "/bme-h", "/bme-p"], // Дополнится к base_topic
      },
      dht22: {
        enabled: false,
        pins: [15],
        labels: ["Температура", "Влажность"],
        units: ["°C", "%"],
        ui_cards: ["card-dht-t", "card-dht-h"],
        topics: ["/dht-t", "/dht-h"],
      },
      ds18b20: {
        enabled: true,
        pins: [4],
        addresses: ["", "", "", ""], // MAC адреса датчиков
        labels: ["Улица", "Дом", "Погреб", "Котел"],
        units: ["°C"],
        ui_cards: ["card-t1", "card-t2", "card-t3", "card-t4"],
        topics: ["/t1", "/t2", "/t3", "/t4"],
      },
      tcrt5000: {
        enabled: false,
        pins: [36],
        labels: ["Освещение (TCRT)"],
        ui_cards: ["card-lux-tcrt"],
        topics: ["/lux"],
      },
    },

    // Дискретные датчики (Binary: 0 или 1)
    binary: {
      pir: {
        enabled: false,
        pin: 35,
        type: "motion",
        label: "Движение",
        ui_card: "card-pir",
        topic: "/motion",
      },
      ld2420: {
        enabled: false,
        pin: 35,
        type: "presence",
        label: "Присутствие",
        ui_card: "card-pres",
        topic: "/presence",
      },
      door: {
        enabled: false,
        pin: 36,
        type: "contact",
        label: "Дверь",
        ui_card: "card-door",
        topic: "/door",
      },
      flood: {
        enabled: false,
        pin: 2,
        type: "leak",
        label: "Затопление",
        ui_card: "card-flood",
        topic: "/flood",
      },
    },

    // Аналоговые датчики (ADC)
    analog: {
      light_resistor: {
        enabled: false,
        pin: 39,
        type: "light",
        label: "Освещение (LDR)",
        ui_card: "card-lux-5516",
        topic: "/lux_raw",
      },
    },

    // Исполнительные устройства
    actuators: {
      relays: {
        enabled: false,
        pins: [26, 27, 14, 13],
        labels: ["Розетка 1", "Розетка 2", "Розетка 3", "Розетка 4"],
        ui_cards: ["card-r0", "card-r1", "card-r2", "card-r3"],
        topics: ["/r0", "/r1", "/r2", "/r3"],
      },
    },
  },
};
