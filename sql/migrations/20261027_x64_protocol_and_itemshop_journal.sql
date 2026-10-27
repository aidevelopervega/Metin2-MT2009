-- ============================================================================
-- MT2009 x64 Protocol & ItemShop Durable Transaction Journal Migration
-- Target Engine: MariaDB 12.3 LTS
-- Protocol Version: MT2_PROTOCOL_VERSION_X64 (0x20090064)
-- ============================================================================

SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';

-- 1. Account Schema: 64-bit Dragon Coin / Dragon Mark balances & Durable ItemShop Journal
ALTER TABLE `account`.`account`
  MODIFY COLUMN `cash` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `cash_mark` bigint(20) NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS `account`.`itemshop_tx_journal` (
  `tx_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `aid` int(11) unsigned NOT NULL DEFAULT 0,
  `pid` int(11) unsigned NOT NULL DEFAULT 0,
  `login` varchar(30) NOT NULL DEFAULT '',
  `item_index` int(11) NOT NULL DEFAULT 0,
  `vnum` int(11) unsigned NOT NULL DEFAULT 0,
  `quantity` int(11) NOT NULL DEFAULT 1,
  `item_count` int(11) unsigned NOT NULL DEFAULT 1,
  `charge_type` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `cash_delta` bigint(20) NOT NULL DEFAULT 0,
  `mark_delta` bigint(20) NOT NULL DEFAULT 0,
  `money_before_buy` bigint(20) NOT NULL DEFAULT 0,
  `is_vip` tinyint(1) NOT NULL DEFAULT 0,
  `vip_type` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `vip_hours` int(11) unsigned NOT NULL DEFAULT 0,
  `is_time_auction` tinyint(1) NOT NULL DEFAULT 0,
  `auction_reserved_count` int(11) unsigned NOT NULL DEFAULT 0,
  `state` enum('INITIATED','CHARGED','DELIVERED','REFUNDED','NEEDS_MANUAL_REVIEW') NOT NULL DEFAULT 'INITIATED',
  `delivered_item_ids` varchar(255) NOT NULL DEFAULT '',
  `created_at` datetime NOT NULL DEFAULT current_timestamp(),
  `updated_at` datetime NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp(),
  PRIMARY KEY (`tx_id`),
  KEY `idx_itemshop_tx_state` (`state`),
  KEY `idx_itemshop_tx_aid` (`aid`),
  KEY `idx_itemshop_tx_pid` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;

-- 2. Common Schema: 64-bit ItemShop Prices & Voucher Cash
ALTER TABLE `common`.`itemshop_items`
  MODIFY COLUMN `price` bigint(20) NOT NULL;

ALTER TABLE `common`.`itemshop_promotions`
  MODIFY COLUMN `price` bigint(20) NOT NULL;

ALTER TABLE `common`.`vouchers`
  MODIFY COLUMN `cash` bigint(20) unsigned NOT NULL DEFAULT 0;

-- 3. Log Schema: 64-bit ItemShop Balance & Price Logs
CREATE TABLE IF NOT EXISTS `log`.`itemshop` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `account_id` int(10) unsigned NOT NULL DEFAULT 0,
  `item_index` int(11) NOT NULL DEFAULT 0,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `count` int(11) NOT NULL DEFAULT 0,
  `price` bigint(20) NOT NULL DEFAULT 0,
  `currency` tinyint(4) NOT NULL DEFAULT 0,
  `item_id` int(10) unsigned DEFAULT NULL,
  `date` datetime NOT NULL DEFAULT current_timestamp(),
  `money_before_buy` bigint(20) NOT NULL DEFAULT 0,
  KEY `idx_itemshop_pid` (`pid`),
  KEY `idx_itemshop_account_id` (`account_id`),
  KEY `idx_itemshop_date` (`date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;

ALTER TABLE `log`.`itemshop`
  MODIFY COLUMN `price` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `money_before_buy` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `log`.`itemshop_log`
  MODIFY COLUMN `price` bigint(20) DEFAULT NULL;

-- 4. Player Schema: 64-bit Timestamps, Economy (YANG), Affect Durations/Costs, Special Flags & ItemAward Uniqueness
ALTER TABLE `player`.`affect`
  MODIFY COLUMN `lDuration` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `lSPCost` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild`
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `token` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild_member`
  MODIFY COLUMN `offer` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `player`.`guild_war_bet`
  MODIFY COLUMN `gold` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild_war_reservation`
  MODIFY COLUMN `warprice` bigint(20) unsigned NOT NULL DEFAULT 0;

UPDATE `player`.`item_award`
  SET `why` = CONCAT(LEFT(`why`, 96), '#LEGACY_', `id`)
  WHERE `why` IS NOT NULL AND `why` NOT LIKE 'ITEMSHOP_TX_%' AND `why` NOT LIKE '%#LEGACY_%';

ALTER TABLE `player`.`item_award`
  ADD UNIQUE KEY IF NOT EXISTS `idx_item_award_why` (`why`) USING BTREE;

ALTER TABLE `player`.`marriage`
  MODIFY COLUMN `time` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`monarch`
  MODIFY COLUMN `money` bigint(20) DEFAULT NULL;

ALTER TABLE `player`.`myshop_pricelist`
  MODIFY COLUMN `price` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`player`
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `horse_hp_droptime` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`player_deleted`
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `horse_hp_droptime` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`player_special_flag`
  MODIFY COLUMN `value` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`safebox`
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0;

-- 5. World Schema: 64-bit Prototype Prices, Gold Rewards & Refine Costs
ALTER TABLE `world`.`crafting_proto`
  MODIFY COLUMN `price` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `world`.`item_proto`
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `shop_buy_price` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `world`.`land`
  MODIFY COLUMN `price` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `world`.`object_proto`
  MODIFY COLUMN `price` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `world`.`quest_reward_proto`
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `world`.`refine_proto`
  MODIFY COLUMN `cost` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `world`.`shop_special_proto`
  MODIFY COLUMN `price` bigint(20) NOT NULL DEFAULT 0;
