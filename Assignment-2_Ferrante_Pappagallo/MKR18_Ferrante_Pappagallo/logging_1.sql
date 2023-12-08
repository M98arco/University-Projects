
SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";

--
-- Database: `logging`
--
CREATE DATABASE IF NOT EXISTS `RiccardoFerrante` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `RiccardoFerrante`;

-- --------------------------------------------------------

--
-- Struttura della tabella `wifi_data_simple`
--

DROP TABLE IF EXISTS `wifi_data_simple_1`;
CREATE TABLE `wifi_data_simple_1` (
  `id` int(11) NOT NULL,
  `datetime` timestamp NULL DEFAULT current_timestamp(),
  `ssid` varchar(45) DEFAULT NULL,
  `rssi` int(11) DEFAULT NULL,
  `led_status` int(1) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DROP TABLE IF EXISTS `TempLightSensor_1`;
CREATE TABLE `TempLightSensor_1` (
  `id` int(11) NOT NULL,
  `datetime` timestamp NULL DEFAULT current_timestamp(),
  `temp` int(11) DEFAULT NULL,
  `light` int(1) DEFAULT NULL,
  `temp_status` int(1) DEFAULT NULL,
  `light_status` int(1) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Indici per le tabelle `wifi_data_simple`
--
ALTER TABLE `wifi_data_simple_1`
  ADD PRIMARY KEY (`id`);
  
ALTER TABLE `TempLightSensor_1`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT per la tabella `wifi_data_simple`
--
ALTER TABLE `wifi_data_simple_1`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT;
  
ALTER TABLE `TempLightSensor_1`
  MODIFY `id` Int(11) NOT NULL AUTO_INCREMENT;


COMMIT;