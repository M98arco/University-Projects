
SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";

--
-- Database: `logging`
--
CREATE DATABASE IF NOT EXISTS `RiccardoFerrante` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `RiccardoFerrante`;

-- --------------------------------wifi_data_simplewifi_data_simplewifi_data_simplewifi_data_simple------------------------

--
-- Struttura della tabella `wifi_data_simple`
--

DROP TABLE IF EXISTS `wifi_data_simple`;
CREATE TABLE `wifi_data_simple` (
  `id` int(11) NOT NULL,
  `datetime` timestamp NULL DEFAULT current_timestamp(),
  `ssid` varchar(45) DEFAULT NULL,
  `rssi` int(11) DEFAULT NULL,
  `led_status` int(1) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DROP TABLE IF EXISTS TempLightSensor;
CREATE TABLE TempLightSensor (
  id int(11) NOT NULL AUTO_INCREMENT,
  datetime timestamp NULL DEFAULT current_timestamp(),
  temp int(11) DEFAULT NULL,
  light int(1) DEFAULT NULL,
  PRIMARY KEY(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


COMMIT;