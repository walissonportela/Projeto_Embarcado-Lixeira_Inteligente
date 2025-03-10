# 📌 Lixeira Inteligente - Sistemas Embarcados

## 📌 Autor
👤 **Joaquim Walisson Portela de Sousa**  
📚 **Disciplina:** Sistemas Embarcados  
📅 **Ano:** 2025  

## 📖 Sobre o Projeto
Este projeto consiste no desenvolvimento de uma **lixeira inteligente** utilizando a plataforma **ESP32**, sensores e atuadores controlados por meio do **FreeRTOS**.

O sistema monitora variáveis como **temperatura, umidade, nível de gás, peso da lixeira e capacidade de armazenamento**, acionando **alertas visuais e sonoros** caso algum limite seja excedido.

## 🖥️ Tecnologias Utilizadas
- **Plataforma:** ESP32
- **Sistema Operacional:** FreeRTOS
- **Simulador:** Wokwi + PlatformIO (VSCode)
- **Linguagem:** C++

## 📌 Sensores e Atuadores Utilizados
- **Sensores:**
  - **BMP280 (SPI)** → Temperatura
  - **DHT22 (I2C)** → Umidade
  - **MQ-2 (Analógico)** → Gás inflamável
  - **HC-SR04 (Digital)** → Capacidade da lixeira
  - **MPU6050 (I2C)** → Detecção de quedas
  - **Potenciômetro (Analógico)** → Simulação de peso
- **Atuadores:**
  - **Buzzer** → Alarme sonoro
  - **OLED** → Tela para apresentar as medições
  - **LEDs RGB** → Indicadores de status

## 🚀 Como Rodar o Projeto
### 📌 1️⃣ Clonar o Repositório
```bash
git clone https://github.com/walissonportela/Projeto_Embarcado-Lixeira_Inteligente.git
cd Projeto_Embarcado-Lixeira_Inteligente
```

### 📌 2️⃣ Abrir no VSCode com PlatformIO
1. **Baixe e instale o [VSCode]**
2. **Instale a extensão [PlatformIO]**
3. **Instale a extensão [Wokwi]**
4. **Abra a pasta do projeto no VSCode**
5. **Compile e rode o código!** ✅
