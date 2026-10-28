-- =============================================================================
-- MT2009 Rev 52 — Additive Log Schema Migration (ikarusshop_log & itemshop)
-- =============================================================================
-- Purpose:
--   1. Create `log.ikarusshop_log` required by `CClientManager::IkarusShopLog`
--      (`server/src/db/src/ClientManagerIkarusShop.cpp`) when `ENABLE_IKASHOP_LOGS`
--      is enabled (`server/src/common/CommonDefines.h`).
--   2. Create `log.itemshop` (10 columns) required by `CItemShopManager::BuyItem`
--      (`server/src/game/src/itemshop_manager.cpp`) while preserving existing
--      `log.itemshop_log` without modification or data loss.
--
-- Safety:
--   - Strictly additive (`CREATE TABLE IF NOT EXISTS`).
--   - Does NOT drop, truncate, or alter any existing table or column.
--   - Uses `ENGINE=InnoDB` for `--single-transaction` backup consistency.
--   - Compatible with MariaDB 12.3 LTS strict `sql_mode`.
-- =============================================================================

CREATE TABLE IF NOT EXISTS `ikarusshop_log` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `who` bigint(20) unsigned NOT NULL DEFAULT 0,
  `itemid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `what` varchar(64) NOT NULL DEFAULT '',
  `shop_owner` bigint(20) unsigned NOT NULL DEFAULT 0,
  `extra` text NOT NULL,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `count` int(10) unsigned NOT NULL DEFAULT 0,
  `yang` bigint(20) NOT NULL DEFAULT 0,
  `cheque` int(11) NOT NULL DEFAULT 0,
  `log_date` datetime NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`id`),
  KEY `idx_ikarusshop_log_who` (`who`),
  KEY `idx_ikarusshop_log_owner` (`shop_owner`),
  KEY `idx_ikarusshop_log_itemid` (`itemid`),
  KEY `idx_ikarusshop_log_date` (`log_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `itemshop` (
  `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `account_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  `item_index` int(11) NOT NULL DEFAULT 0,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `count` int(11) NOT NULL DEFAULT 0,
  `price` bigint(20) NOT NULL DEFAULT 0,
  `currency` tinyint(4) NOT NULL DEFAULT 0,
  `item_id` bigint(20) unsigned DEFAULT NULL,
  `date` datetime NOT NULL DEFAULT current_timestamp(),
  `money_before_buy` bigint(20) NOT NULL DEFAULT 0,
  KEY `idx_itemshop_pid` (`pid`),
  KEY `idx_itemshop_account_id` (`account_id`),
  KEY `idx_itemshop_date` (`date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;
