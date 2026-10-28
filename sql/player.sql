/*M!999999\- enable the sandbox mode */ 
-- MariaDB dump 10.19-11.8.2-MariaDB, for FreeBSD14.2 (amd64)
--
-- Host: localhost    Database: player
-- ------------------------------------------------------
-- Server version	11.8.2-MariaDB

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*M!100616 SET @OLD_NOTE_VERBOSITY=@@NOTE_VERBOSITY, NOTE_VERBOSITY=0 */;

--
-- Table structure for table `affect`
--

DROP TABLE IF EXISTS `affect`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `affect` (
  `dwPID` bigint(20) unsigned NOT NULL DEFAULT 0,
  `bType` smallint(5) unsigned NOT NULL DEFAULT 0,
  `bApplyOn` tinyint(4) unsigned NOT NULL DEFAULT 0,
  `lApplyValue` int(11) NOT NULL DEFAULT 0,
  `dwFlag` int(10) unsigned NOT NULL DEFAULT 0,
  `lDuration` bigint(20) NOT NULL DEFAULT 0,
  `lSPCost` bigint(20) NOT NULL DEFAULT 0,
  `itemVnum` int(10) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`dwPID`,`bType`,`bApplyOn`,`lApplyValue`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `affect`
--

LOCK TABLES `affect` WRITE;
/*!40000 ALTER TABLE `affect` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `affect` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `change_empire`
--

DROP TABLE IF EXISTS `change_empire`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `change_empire` (
  `aid` bigint(20) unsigned NOT NULL,
  `change_count` int(10) unsigned NOT NULL DEFAULT 0,
  `last_time` datetime NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp(),
  PRIMARY KEY (`aid`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `change_empire`
--

LOCK TABLES `change_empire` WRITE;
/*!40000 ALTER TABLE `change_empire` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `change_empire` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild`
--

DROP TABLE IF EXISTS `guild`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(14) NOT NULL DEFAULT '' COMMENT 'snprintf(12u)',
  `sp` smallint(6) NOT NULL DEFAULT 1000,
  `master` bigint(20) unsigned NOT NULL DEFAULT 0,
  `level` tinyint(2) DEFAULT NULL,
  `exp` int(11) DEFAULT NULL,
  `skill_point` tinyint(2) NOT NULL DEFAULT 0,
  `skill` tinyblob DEFAULT NULL,
  `win` int(11) NOT NULL DEFAULT 0,
  `draw` int(11) NOT NULL DEFAULT 0,
  `loss` int(11) NOT NULL DEFAULT 0,
  `ladder_point` int(11) NOT NULL DEFAULT 0,
  `gold` bigint(20) NOT NULL DEFAULT 0,
  `token` bigint(20) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `guild`
--

LOCK TABLES `guild` WRITE;
/*!40000 ALTER TABLE `guild` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `guild` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild_comment`
--

DROP TABLE IF EXISTS `guild_comment`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_comment` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `guild_id` bigint(20) unsigned DEFAULT NULL,
  `name` varchar(24) DEFAULT NULL,
  `notice` tinyint(4) DEFAULT NULL,
  `content` varchar(50) DEFAULT NULL,
  `time` datetime DEFAULT NULL,
  PRIMARY KEY (`id`) USING BTREE,
  KEY `aaa` (`notice`,`id`,`guild_id`) USING BTREE,
  KEY `guild_id` (`guild_id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=cp1250 COLLATE=cp1250_polish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `guild_comment`
--

LOCK TABLES `guild_comment` WRITE;
/*!40000 ALTER TABLE `guild_comment` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `guild_comment` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild_grade`
--

DROP TABLE IF EXISTS `guild_grade`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_grade` (
  `guild_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `grade` tinyint(4) NOT NULL DEFAULT 0,
  `name` varchar(12) NOT NULL DEFAULT '' COMMENT 'strlen(s) <= 12',
  `auth` set('ADD_MEMBER','REMOVE_MEMEBER','NOTICE','USE_SKILL') CHARACTER SET latin1 COLLATE latin1_swedish_ci DEFAULT NULL,
  PRIMARY KEY (`guild_id`,`grade`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=cp1250 COLLATE=cp1250_polish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `guild_grade`
--

LOCK TABLES `guild_grade` WRITE;
/*!40000 ALTER TABLE `guild_grade` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `guild_grade` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild_land`
--

DROP TABLE IF EXISTS `guild_land`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_land` (
  `land_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `guild_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`land_id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `guild_land`
--

LOCK TABLES `guild_land` WRITE;
/*!40000 ALTER TABLE `guild_land` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `guild_land` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild_member`
--

DROP TABLE IF EXISTS `guild_member`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_member` (
  `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `guild_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `grade` tinyint(2) DEFAULT NULL,
  `is_general` tinyint(1) NOT NULL DEFAULT 0,
  `offer` bigint(20) unsigned DEFAULT NULL,
  PRIMARY KEY (`guild_id`,`pid`) USING BTREE,
  UNIQUE KEY `pid` (`pid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `guild_member`
--

LOCK TABLES `guild_member` WRITE;
/*!40000 ALTER TABLE `guild_member` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `guild_member` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild_war`
--

DROP TABLE IF EXISTS `guild_war`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_war` (
  `id_from` bigint(20) unsigned NOT NULL DEFAULT 0,
  `id_to` bigint(20) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id_from`,`id_to`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `guild_war`
--

LOCK TABLES `guild_war` WRITE;
/*!40000 ALTER TABLE `guild_war` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `guild_war` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild_war_bet`
--

DROP TABLE IF EXISTS `guild_war_bet`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_war_bet` (
  `login` varchar(30) NOT NULL DEFAULT '',
  `gold` bigint(20) unsigned NOT NULL DEFAULT 0,
  `guild` bigint(20) unsigned NOT NULL DEFAULT 0,
  `war_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`war_id`,`login`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `guild_war_bet`
--

LOCK TABLES `guild_war_bet` WRITE;
/*!40000 ALTER TABLE `guild_war_bet` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `guild_war_bet` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild_war_reservation`
--

DROP TABLE IF EXISTS `guild_war_reservation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_war_reservation` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `guild1` bigint(20) unsigned NOT NULL DEFAULT 0,
  `guild2` bigint(20) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `type` tinyint(2) unsigned NOT NULL DEFAULT 0,
  `warprice` bigint(20) unsigned NOT NULL DEFAULT 0,
  `initscore` int(10) unsigned NOT NULL DEFAULT 0,
  `started` tinyint(1) NOT NULL DEFAULT 0,
  `bet_from` int(10) unsigned NOT NULL DEFAULT 0,
  `bet_to` int(10) unsigned NOT NULL DEFAULT 0,
  `winner` bigint(20) NOT NULL DEFAULT -1,
  `power1` int(11) NOT NULL DEFAULT 0,
  `power2` int(11) NOT NULL DEFAULT 0,
  `handicap` int(11) NOT NULL DEFAULT 0,
  `result1` int(11) NOT NULL DEFAULT 0,
  `result2` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `guild_war_reservation`
--

LOCK TABLES `guild_war_reservation` WRITE;
/*!40000 ALTER TABLE `guild_war_reservation` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `guild_war_reservation` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `horse_name`
--

DROP TABLE IF EXISTS `horse_name`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `horse_name` (
  `id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `name` varchar(16) NOT NULL DEFAULT 'NONAME' COMMENT 'CHARACTER_NAME_MAX_LEN+1 so 24+null',
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `horse_name`
--

LOCK TABLES `horse_name` WRITE;
/*!40000 ALTER TABLE `horse_name` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `horse_name` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `ikashop_auction_offer`
--

DROP TABLE IF EXISTS `ikashop_auction_offer`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `ikashop_auction_offer` (
  `seller` bigint(20) unsigned NOT NULL,
  `buyer` bigint(20) unsigned NOT NULL,
  `buyer_name` varchar(30) NOT NULL,
  `price` bigint(20) unsigned NOT NULL,
  `datetime` datetime NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp(),
  `seen` tinyint(3) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`seller`,`buyer`,`price`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `ikashop_auction_offer`
--

LOCK TABLES `ikashop_auction_offer` WRITE;
/*!40000 ALTER TABLE `ikashop_auction_offer` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `ikashop_auction_offer` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `ikashop_notification`
--

DROP TABLE IF EXISTS `ikashop_notification`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `ikashop_notification` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `owner` bigint(20) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL,
  `who` varchar(32) NOT NULL,
  `what` int(10) unsigned NOT NULL,
  `format` varchar(50) NOT NULL,
  `seen` tinyint(4) NOT NULL DEFAULT 0,
  `datetime` datetime NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `ikashop_notification`
--

LOCK TABLES `ikashop_notification` WRITE;
/*!40000 ALTER TABLE `ikashop_notification` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `ikashop_notification` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `ikashop_offlineshop`
--

DROP TABLE IF EXISTS `ikashop_offlineshop`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `ikashop_offlineshop` (
  `owner` bigint(20) unsigned NOT NULL,
  `duration` int(10) unsigned NOT NULL DEFAULT 0,
  `map` int(10) unsigned NOT NULL DEFAULT 0,
  `x` int(10) unsigned NOT NULL DEFAULT 0,
  `y` int(10) unsigned NOT NULL DEFAULT 0,
  `channel` tinyint(3) NOT NULL DEFAULT 1,
  `decoration` int(10) unsigned NOT NULL DEFAULT 0,
  `decoration_time` int(10) unsigned NOT NULL DEFAULT 0,
  `lock_index` int(10) unsigned NOT NULL DEFAULT 0,
  `name` varchar(32) NOT NULL DEFAULT 'NONAME',
  `unlock_count` tinyint(3) NOT NULL DEFAULT 0,
  `is_premium` tinyint(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=cp1250 COLLATE=cp1250_polish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `ikashop_offlineshop`
--

LOCK TABLES `ikashop_offlineshop` WRITE;
/*!40000 ALTER TABLE `ikashop_offlineshop` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `ikashop_offlineshop` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `ikashop_private_offer`
--

DROP TABLE IF EXISTS `ikashop_private_offer`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `ikashop_private_offer` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `seller` bigint(20) unsigned NOT NULL,
  `buyer` bigint(20) unsigned NOT NULL,
  `item` bigint(20) unsigned NOT NULL,
  `price` bigint(20) unsigned NOT NULL,
  `notified` tinyint(3) unsigned NOT NULL,
  `accepted` tinyint(3) unsigned NOT NULL,
  `buyer_name` varchar(30) NOT NULL,
  `datetime` datetime DEFAULT current_timestamp(),
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `ikashop_private_offer`
--

LOCK TABLES `ikashop_private_offer` WRITE;
/*!40000 ALTER TABLE `ikashop_private_offer` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `ikashop_private_offer` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `ikashop_safebox`
--

DROP TABLE IF EXISTS `ikashop_safebox`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `ikashop_safebox` (
  `owner` bigint(20) unsigned NOT NULL,
  `gold` bigint(20) unsigned NOT NULL,
  `cheque` int(10) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `ikashop_safebox`
--

LOCK TABLES `ikashop_safebox` WRITE;
/*!40000 ALTER TABLE `ikashop_safebox` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `ikashop_safebox` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `ikashop_sale`
--

DROP TABLE IF EXISTS `ikashop_sale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `ikashop_sale` (
  `id` bigint(20) unsigned NOT NULL,
  `vnum` int(10) unsigned NOT NULL,
  `count` int(10) unsigned NOT NULL,
  `price` bigint(20) unsigned NOT NULL,
  `datetime` datetime NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `ikashop_sale`
--

LOCK TABLES `ikashop_sale` WRITE;
/*!40000 ALTER TABLE `ikashop_sale` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `ikashop_sale` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `item`
--

DROP TABLE IF EXISTS `item`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `item` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `owner_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `window` enum('INVENTORY','EQUIPMENT','SAFEBOX','MALL','DRAGON_SOUL_INVENTORY','BELT_INVENTORY','GROUND','IKASHOP_OFFLINESHOP','IKASHOP_SAFEBOX','IKASHOP_AUCTION') NOT NULL,
  `pos` smallint(5) unsigned NOT NULL DEFAULT 0,
  `count` int(10) unsigned NOT NULL DEFAULT 0,
  `vnum` int(11) unsigned NOT NULL DEFAULT 0,
  `socket0` int(10) unsigned NOT NULL DEFAULT 0,
  `socket1` int(10) unsigned NOT NULL DEFAULT 0,
  `socket2` int(10) unsigned NOT NULL DEFAULT 0,
  `attrtype0` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue0` smallint(6) NOT NULL DEFAULT 0,
  `attrtype1` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue1` smallint(6) NOT NULL DEFAULT 0,
  `attrtype2` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue2` smallint(6) NOT NULL DEFAULT 0,
  `attrtype3` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue3` smallint(6) NOT NULL DEFAULT 0,
  `attrtype4` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue4` smallint(6) NOT NULL DEFAULT 0,
  `attrtype5` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue5` smallint(6) NOT NULL DEFAULT 0,
  `attrtype6` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue6` smallint(6) NOT NULL DEFAULT 0,
  `ikashop_data` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`) USING BTREE,
  KEY `owner_id_idx` (`owner_id`) USING BTREE,
  KEY `item_vnum_index` (`vnum`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `item`
--

LOCK TABLES `item` WRITE;
/*!40000 ALTER TABLE `item` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `item` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `item_award`
--

DROP TABLE IF EXISTS `item_award`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_award` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `login` varchar(30) NOT NULL DEFAULT '' COMMENT 'LOGIN_MAX_LEN=30',
  `vnum` int(6) unsigned NOT NULL DEFAULT 0,
  `count` int(10) unsigned NOT NULL DEFAULT 1,
  `given_time` datetime NOT NULL DEFAULT current_timestamp(),
  `taken_time` datetime DEFAULT NULL,
  `item_id` bigint(20) unsigned DEFAULT NULL,
  `why` varchar(128) DEFAULT NULL,
  `socket0` int(11) unsigned NOT NULL DEFAULT 0,
  `socket1` int(11) unsigned NOT NULL DEFAULT 0,
  `socket2` int(11) unsigned NOT NULL DEFAULT 0,
  `attrtype0` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue0` smallint(6) NOT NULL DEFAULT 0,
  `attrtype1` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue1` smallint(6) NOT NULL DEFAULT 0,
  `attrtype2` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue2` smallint(6) NOT NULL DEFAULT 0,
  `attrtype3` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue3` smallint(6) NOT NULL DEFAULT 0,
  `attrtype4` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue4` smallint(6) NOT NULL DEFAULT 0,
  `attrtype5` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue5` smallint(6) NOT NULL DEFAULT 0,
  `attrtype6` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `attrvalue6` smallint(6) NOT NULL DEFAULT 0,
  `mall` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE KEY `idx_item_award_why` (`why`) USING BTREE,
  KEY `given_time_idx` (`given_time`) USING BTREE,
  KEY `taken_time_idx` (`taken_time`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `item_award`
--

LOCK TABLES `item_award` WRITE;
/*!40000 ALTER TABLE `item_award` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `item_award` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `itemshop_time_auction`
--

DROP TABLE IF EXISTS `itemshop_time_auction`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `itemshop_time_auction` (
  `item_index` int(10) NOT NULL,
  `buy_count` int(10) NOT NULL DEFAULT 0,
  PRIMARY KEY (`item_index`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_general_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `itemshop_time_auction`
--

LOCK TABLES `itemshop_time_auction` WRITE;
/*!40000 ALTER TABLE `itemshop_time_auction` DISABLE KEYS */;
set autocommit=0;
INSERT INTO `itemshop_time_auction` VALUES
(906,0),
(907,0),
(908,0);
/*!40000 ALTER TABLE `itemshop_time_auction` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `lotto_list`
--

DROP TABLE IF EXISTS `lotto_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `lotto_list` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `server` varchar(56) DEFAULT NULL COMMENT 'server%s=get_table_postfix(); std::string so dunno; at least 6',
  `pid` bigint(20) unsigned DEFAULT NULL,
  `time` datetime DEFAULT NULL,
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `lotto_list`
--

LOCK TABLES `lotto_list` WRITE;
/*!40000 ALTER TABLE `lotto_list` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `lotto_list` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `marriage`
--

DROP TABLE IF EXISTS `marriage`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `marriage` (
  `is_married` tinyint(4) NOT NULL DEFAULT 0,
  `pid1` bigint(20) unsigned NOT NULL DEFAULT 0,
  `pid2` bigint(20) unsigned NOT NULL DEFAULT 0,
  `love_point` int(11) unsigned DEFAULT NULL,
  `time` bigint(20) NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid1`,`pid2`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `marriage`
--

LOCK TABLES `marriage` WRITE;
/*!40000 ALTER TABLE `marriage` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `marriage` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `messenger_block_list`
--

DROP TABLE IF EXISTS `messenger_block_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `messenger_block_list` (
  `pid` bigint(20) unsigned NOT NULL,
  `blocked` varchar(24) NOT NULL,
  PRIMARY KEY (`pid`,`blocked`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=cp1250 COLLATE=cp1250_general_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `messenger_block_list`
--

LOCK TABLES `messenger_block_list` WRITE;
/*!40000 ALTER TABLE `messenger_block_list` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `messenger_block_list` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `messenger_list`
--

DROP TABLE IF EXISTS `messenger_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `messenger_list` (
  `account` varchar(24) NOT NULL DEFAULT '' COMMENT '24 at maximum',
  `companion` varchar(24) NOT NULL DEFAULT '' COMMENT '24 at maximum',
  PRIMARY KEY (`account`,`companion`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `messenger_list`
--

LOCK TABLES `messenger_list` WRITE;
/*!40000 ALTER TABLE `messenger_list` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `messenger_list` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `monarch`
--

DROP TABLE IF EXISTS `monarch`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `monarch` (
  `empire` int(10) unsigned NOT NULL DEFAULT 0,
  `pid` bigint(20) unsigned DEFAULT NULL,
  `windate` datetime DEFAULT NULL,
  `money` bigint(20) DEFAULT NULL,
  PRIMARY KEY (`empire`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `monarch`
--

LOCK TABLES `monarch` WRITE;
/*!40000 ALTER TABLE `monarch` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `monarch` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `monarch_candidacy`
--

DROP TABLE IF EXISTS `monarch_candidacy`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `monarch_candidacy` (
  `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `date` datetime DEFAULT current_timestamp(),
  `name` varchar(24) DEFAULT NULL,
  `windate` datetime DEFAULT NULL,
  PRIMARY KEY (`pid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `monarch_candidacy`
--

LOCK TABLES `monarch_candidacy` WRITE;
/*!40000 ALTER TABLE `monarch_candidacy` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `monarch_candidacy` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `monarch_election`
--

DROP TABLE IF EXISTS `monarch_election`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `monarch_election` (
  `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `selectedpid` bigint(20) unsigned DEFAULT 0,
  `electiondata` datetime DEFAULT current_timestamp(),
  PRIMARY KEY (`pid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `monarch_election`
--

LOCK TABLES `monarch_election` WRITE;
/*!40000 ALTER TABLE `monarch_election` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `monarch_election` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `myshop_pricelist`
--

DROP TABLE IF EXISTS `myshop_pricelist`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `myshop_pricelist` (
  `owner_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `item_vnum` int(11) unsigned NOT NULL DEFAULT 0,
  `price` bigint(20) NOT NULL DEFAULT 0,
  UNIQUE KEY `list_id` (`owner_id`,`item_vnum`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `myshop_pricelist`
--

LOCK TABLES `myshop_pricelist` WRITE;
/*!40000 ALTER TABLE `myshop_pricelist` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `myshop_pricelist` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `object`
--

DROP TABLE IF EXISTS `object`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `object` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `land_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `map_index` int(11) unsigned NOT NULL DEFAULT 0,
  `x` int(11) NOT NULL DEFAULT 0,
  `y` int(11) NOT NULL DEFAULT 0,
  `x_rot` float NOT NULL DEFAULT 0,
  `y_rot` float NOT NULL DEFAULT 0,
  `z_rot` float NOT NULL DEFAULT 0,
  `life` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `object`
--

LOCK TABLES `object` WRITE;
/*!40000 ALTER TABLE `object` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `object` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `pcbang_ip`
--

DROP TABLE IF EXISTS `pcbang_ip`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `pcbang_ip` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `pcbang_id` int(11) NOT NULL DEFAULT 0,
  `ip` varchar(15) NOT NULL DEFAULT '000.000.000.000',
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE KEY `ip` (`ip`) USING BTREE,
  KEY `pcbang_id` (`pcbang_id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `pcbang_ip`
--

LOCK TABLES `pcbang_ip` WRITE;
/*!40000 ALTER TABLE `pcbang_ip` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `pcbang_ip` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `player`
--

DROP TABLE IF EXISTS `player`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `player` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `account_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `name` varchar(24) NOT NULL DEFAULT 'NONAME',
  `job` tinyint(2) unsigned NOT NULL DEFAULT 0,
  `voice` tinyint(1) NOT NULL DEFAULT 0,
  `dir` tinyint(2) NOT NULL DEFAULT 0,
  `x` int(11) NOT NULL DEFAULT 0,
  `y` int(11) NOT NULL DEFAULT 0,
  `z` int(11) NOT NULL DEFAULT 0,
  `map_index` int(11) NOT NULL DEFAULT 0,
  `exit_x` int(11) NOT NULL DEFAULT 0,
  `exit_y` int(11) NOT NULL DEFAULT 0,
  `exit_map_index` int(11) NOT NULL DEFAULT 0,
  `hp` int(11) NOT NULL DEFAULT 0,
  `mp` int(11) NOT NULL DEFAULT 0,
  `stamina` smallint(6) NOT NULL DEFAULT 0,
  `random_hp` smallint(5) NOT NULL DEFAULT 0 COMMENT 'if lvl 0, it will be negative',
  `random_sp` smallint(5) NOT NULL DEFAULT 0 COMMENT 'if lvl 0, it will be negative',
  `playtime` int(11) NOT NULL DEFAULT 0,
  `level` tinyint(2) unsigned NOT NULL DEFAULT 1,
  `level_step` tinyint(1) NOT NULL DEFAULT 0,
  `st` smallint(3) NOT NULL DEFAULT 0,
  `ht` smallint(3) NOT NULL DEFAULT 0,
  `dx` smallint(3) NOT NULL DEFAULT 0,
  `iq` smallint(3) NOT NULL DEFAULT 0,
  `exp` int(11) NOT NULL DEFAULT 0,
  `gold` bigint(20) NOT NULL DEFAULT 0,
  `cheque` int(11) NOT NULL DEFAULT 0,
  `stat_point` smallint(3) NOT NULL DEFAULT 0,
  `skill_point` smallint(3) NOT NULL DEFAULT 0,
  `quickslot` tinyblob DEFAULT NULL,
  `ip` varchar(15) DEFAULT '0.0.0.0',
  `part_main` int(10) unsigned NOT NULL DEFAULT 0,
  `part_base` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `part_hair` int(10) unsigned NOT NULL DEFAULT 0,
  `part_acce` int(11) unsigned NOT NULL DEFAULT 0,
  `skill_group` tinyint(4) NOT NULL DEFAULT 0,
  `skill_level` blob DEFAULT NULL,
  `alignment` int(11) NOT NULL DEFAULT 0,
  `last_play` datetime NOT NULL DEFAULT current_timestamp(),
  `change_name` tinyint(1) NOT NULL DEFAULT 0,
  `mobile` varchar(24) DEFAULT NULL,
  `sub_skill_point` smallint(3) NOT NULL DEFAULT 0,
  `stat_reset_count` tinyint(4) NOT NULL DEFAULT 0,
  `horse_hp` smallint(4) NOT NULL DEFAULT 0,
  `horse_stamina` smallint(4) NOT NULL DEFAULT 0,
  `horse_level` tinyint(2) unsigned NOT NULL DEFAULT 0,
  `horse_hp_droptime` bigint(20) NOT NULL DEFAULT 0,
  `horse_riding` tinyint(1) NOT NULL DEFAULT 0,
  `horse_skill_point` smallint(3) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`) USING BTREE,
  KEY `account_id_idx` (`account_id`) USING BTREE,
  KEY `name_idx` (`name`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `player`
--

LOCK TABLES `player` WRITE;
/*!40000 ALTER TABLE `player` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `player` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `player_deleted`
--

DROP TABLE IF EXISTS `player_deleted`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_deleted` (
  `id` bigint(20) unsigned NOT NULL,
  `account_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `name` varchar(24) NOT NULL DEFAULT 'NONAME',
  `job` tinyint(2) unsigned NOT NULL DEFAULT 0,
  `voice` tinyint(1) NOT NULL DEFAULT 0,
  `dir` tinyint(2) NOT NULL DEFAULT 0,
  `x` int(11) NOT NULL DEFAULT 0,
  `y` int(11) NOT NULL DEFAULT 0,
  `z` int(11) NOT NULL DEFAULT 0,
  `map_index` int(11) NOT NULL DEFAULT 0,
  `exit_x` int(11) NOT NULL DEFAULT 0,
  `exit_y` int(11) NOT NULL DEFAULT 0,
  `exit_map_index` int(11) NOT NULL DEFAULT 0,
  `hp` int(11) NOT NULL DEFAULT 0,
  `mp` int(11) NOT NULL DEFAULT 0,
  `stamina` smallint(6) NOT NULL DEFAULT 0,
  `random_hp` smallint(5) NOT NULL DEFAULT 0 COMMENT 'if lvl 0, it will be negative',
  `random_sp` smallint(5) NOT NULL DEFAULT 0 COMMENT 'if lvl 0, it will be negative',
  `playtime` int(11) NOT NULL DEFAULT 0,
  `level` tinyint(2) unsigned NOT NULL DEFAULT 1,
  `level_step` tinyint(1) NOT NULL DEFAULT 0,
  `st` smallint(3) NOT NULL DEFAULT 0,
  `ht` smallint(3) NOT NULL DEFAULT 0,
  `dx` smallint(3) NOT NULL DEFAULT 0,
  `iq` smallint(3) NOT NULL DEFAULT 0,
  `exp` int(11) NOT NULL DEFAULT 0,
  `gold` bigint(20) NOT NULL DEFAULT 0,
  `cheque` int(11) NOT NULL DEFAULT 0,
  `stat_point` smallint(3) NOT NULL DEFAULT 0,
  `skill_point` smallint(3) NOT NULL DEFAULT 0,
  `quickslot` tinyblob DEFAULT NULL,
  `ip` varchar(15) DEFAULT '0.0.0.0',
  `part_main` int(10) unsigned NOT NULL DEFAULT 0,
  `part_base` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `part_hair` int(10) unsigned NOT NULL DEFAULT 0,
  `part_acce` int(11) unsigned NOT NULL DEFAULT 0,
  `skill_group` tinyint(4) NOT NULL DEFAULT 0,
  `skill_level` blob DEFAULT NULL,
  `alignment` int(11) NOT NULL DEFAULT 0,
  `last_play` datetime NOT NULL DEFAULT current_timestamp(),
  `change_name` tinyint(1) NOT NULL DEFAULT 0,
  `mobile` varchar(24) DEFAULT NULL,
  `sub_skill_point` smallint(3) NOT NULL DEFAULT 0,
  `stat_reset_count` tinyint(4) NOT NULL DEFAULT 0,
  `horse_hp` smallint(4) NOT NULL DEFAULT 0,
  `horse_stamina` smallint(4) NOT NULL DEFAULT 0,
  `horse_level` tinyint(2) unsigned NOT NULL DEFAULT 0,
  `horse_hp_droptime` bigint(20) NOT NULL DEFAULT 0,
  `horse_riding` tinyint(1) NOT NULL DEFAULT 0,
  `horse_skill_point` smallint(3) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `player_deleted`
--

LOCK TABLES `player_deleted` WRITE;
/*!40000 ALTER TABLE `player_deleted` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `player_deleted` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `player_index`
--

DROP TABLE IF EXISTS `player_index`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_index` (
  `id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `pid1` bigint(20) unsigned NOT NULL DEFAULT 0,
  `pid2` bigint(20) unsigned NOT NULL DEFAULT 0,
  `pid3` bigint(20) unsigned NOT NULL DEFAULT 0,
  `pid4` bigint(20) unsigned NOT NULL DEFAULT 0,
  `pid5` bigint(20) unsigned NOT NULL DEFAULT 0,
  `empire` tinyint(4) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`) USING BTREE,
  KEY `pid1_key` (`pid1`) USING BTREE,
  KEY `pid2_key` (`pid2`) USING BTREE,
  KEY `pid3_key` (`pid3`) USING BTREE,
  KEY `pid4_key` (`pid4`) USING BTREE,
  KEY `pid5_key` (`pid5`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `player_index`
--

LOCK TABLES `player_index` WRITE;
/*!40000 ALTER TABLE `player_index` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `player_index` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `player_special_flag`
--

DROP TABLE IF EXISTS `player_special_flag`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_special_flag` (
  `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `aid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `flag` varchar(32) NOT NULL DEFAULT '',
  `value` bigint(20) NOT NULL DEFAULT 0,
  PRIMARY KEY (`pid`,`flag`,`aid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `player_special_flag`
--

LOCK TABLES `player_special_flag` WRITE;
/*!40000 ALTER TABLE `player_special_flag` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `player_special_flag` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `quest`
--

DROP TABLE IF EXISTS `quest`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest` (
  `dwPID` bigint(20) unsigned NOT NULL DEFAULT 0,
  `szName` varchar(32) NOT NULL DEFAULT '',
  `szState` varchar(64) NOT NULL DEFAULT '',
  `lValue` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`dwPID`,`szName`,`szState`) USING BTREE,
  KEY `pid_idx` (`dwPID`) USING BTREE,
  KEY `name_idx` (`szName`) USING BTREE,
  KEY `state_idx` (`szState`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `quest`
--

LOCK TABLES `quest` WRITE;
/*!40000 ALTER TABLE `quest` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `quest` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `safebox`
--

DROP TABLE IF EXISTS `safebox`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `safebox` (
  `account_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `size` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `password` varchar(6) NOT NULL DEFAULT '',
  `gold` bigint(20) NOT NULL DEFAULT 0,
  PRIMARY KEY (`account_id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `safebox`
--

LOCK TABLES `safebox` WRITE;
/*!40000 ALTER TABLE `safebox` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `safebox` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `sms_pool`
--

DROP TABLE IF EXISTS `sms_pool`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `sms_pool` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `server` int(11) NOT NULL DEFAULT 0,
  `sender` varchar(32) DEFAULT NULL,
  `receiver` varchar(100) NOT NULL DEFAULT '',
  `mobile` varchar(32) DEFAULT NULL,
  `sent` enum('N','Y') NOT NULL DEFAULT 'N',
  `msg` varchar(80) DEFAULT NULL,
  PRIMARY KEY (`id`) USING BTREE,
  KEY `sent_idx` (`sent`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sms_pool`
--

LOCK TABLES `sms_pool` WRITE;
/*!40000 ALTER TABLE `sms_pool` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `sms_pool` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `special_spawn`
--

DROP TABLE IF EXISTS `special_spawn`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `special_spawn` (
  `vnum` int(11) NOT NULL,
  `channel` int(10) NOT NULL DEFAULT 0,
  `kill_time` datetime NOT NULL,
  PRIMARY KEY (`vnum`,`channel`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `special_spawn`
--

LOCK TABLES `special_spawn` WRITE;
/*!40000 ALTER TABLE `special_spawn` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `special_spawn` ENABLE KEYS */;
UNLOCK TABLES;
commit;

--
-- Table structure for table `guild_invite_limit`
--

DROP TABLE IF EXISTS `guild_invite_limit`;
CREATE TABLE `guild_invite_limit` (
  `id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `value` int(10) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;

--
-- Table structure for table `string`
--

DROP TABLE IF EXISTS `string`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `string` (
  `name` varchar(64) NOT NULL DEFAULT '',
  `text` text DEFAULT NULL,
  PRIMARY KEY (`name`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `string`
--

LOCK TABLES `string` WRITE;
/*!40000 ALTER TABLE `string` DISABLE KEYS */;
set autocommit=0;
/*!40000 ALTER TABLE `string` ENABLE KEYS */;
UNLOCK TABLES;
commit;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*M!100616 SET NOTE_VERBOSITY=@OLD_NOTE_VERBOSITY */;

-- Dump completed on 2026-02-07 15:11:05
