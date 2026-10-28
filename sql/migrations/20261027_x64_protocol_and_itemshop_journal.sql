-- ============================================================================
-- MT2009 x64 Protocol & ItemShop Durable Transaction Journal Migration
-- Target Engine: MariaDB 12.3 LTS
-- Protocol Version: MT2_PROTOCOL_VERSION_X64 (0x20090064)
-- ============================================================================

SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';

-- 1. Account Schema: 64-bit Account IDs, Dragon Coin / Dragon Mark balances & Durable ItemShop Journal
ALTER TABLE `account`.`account`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `cash` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `cash_mark` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `account`.`account_block`
  MODIFY COLUMN `account_id` bigint(20) unsigned NOT NULL,
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `who` bigint(20) unsigned NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS `account`.`itemshop_tx_journal` (
  `tx_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `aid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  `req_id` bigint(20) unsigned DEFAULT NULL,
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
  `state` enum('INITIATED','CHARGED','RECOVERING','DELIVERED','REFUNDED','NEEDS_MANUAL_REVIEW') NOT NULL DEFAULT 'INITIATED',
  `delivered_item_ids` varchar(255) NOT NULL DEFAULT '',
  `created_at` datetime NOT NULL DEFAULT current_timestamp(),
  `updated_at` datetime NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp(),
  PRIMARY KEY (`tx_id`),
  UNIQUE KEY `idx_itemshop_tx_aid_req` (`aid`,`req_id`),
  KEY `idx_itemshop_tx_state` (`state`),
  KEY `idx_itemshop_tx_aid` (`aid`),
  KEY `idx_itemshop_tx_pid` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC;

ALTER TABLE `account`.`itemshop_tx_journal`
  MODIFY COLUMN `aid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS `req_id` bigint(20) unsigned DEFAULT NULL AFTER `pid`,
  ADD UNIQUE KEY IF NOT EXISTS `idx_itemshop_tx_aid_req` (`aid`,`req_id`),
  MODIFY COLUMN `state` enum('INITIATED','CHARGED','RECOVERING','DELIVERED','REFUNDED','NEEDS_MANUAL_REVIEW') NOT NULL DEFAULT 'INITIATED';

-- 2. Common Schema: 64-bit IDs, ItemShop Prices & Voucher Cash
ALTER TABLE `common`.`itemshop_items`
  MODIFY COLUMN `price` bigint(20) NOT NULL;

ALTER TABLE `common`.`itemshop_promotions`
  MODIFY COLUMN `price` bigint(20) NOT NULL;

ALTER TABLE `common`.`priv_settings`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL DEFAULT 0 COMMENT 'this is for empire_id, player_id or guild_id';

ALTER TABLE `common`.`vouchers`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `cash` bigint(20) unsigned NOT NULL DEFAULT 0;

-- 3. Log Schema: 64-bit Persistent IDs, ItemShop Balance & Price Logs
CREATE TABLE IF NOT EXISTS `log`.`itemshop` (
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

ALTER TABLE `log`.`itemshop`
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `account_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `item_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `price` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `money_before_buy` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `log`.`itemshop_log`
  MODIFY COLUMN `account_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `price` bigint(20) DEFAULT NULL;

CREATE TABLE IF NOT EXISTS `log`.`ikarusshop_log` (
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

ALTER TABLE `log`.`ikarusshop_log`
  MODIFY COLUMN `who` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `itemid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `shop_owner` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `log`.`log`
  MODIFY COLUMN `who` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `what` bigint(20) DEFAULT NULL;

ALTER TABLE `log`.`change_name`
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `log`.`log_chat`
  MODIFY COLUMN `who_id` bigint(20) unsigned NOT NULL;

ALTER TABLE `log`.`log_chat_priv`
  MODIFY COLUMN `who_id` bigint(20) unsigned NOT NULL,
  MODIFY COLUMN `whom_id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`log_commands`
  MODIFY COLUMN `userid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`log_dungeon`
  MODIFY COLUMN `id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`log_level`
  MODIFY COLUMN `account_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT;

ALTER TABLE `log`.`new_money_log`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `char_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `with_id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`offline_shop`
  MODIFY COLUMN `owner_pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `shop_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `buyer_pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `item_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `gold` bigint(20) DEFAULT NULL,
  MODIFY COLUMN `cheque` bigint(20) DEFAULT NULL;

ALTER TABLE `log`.`okay_event`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`loginlog`
  MODIFY COLUMN `account_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`loginlog2`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `account_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`levellog`
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `account_id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`quest_state_log`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`quest_reward_log`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `value1` bigint(20) DEFAULT NULL;

ALTER TABLE `log`.`gold_session_log`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `total_gold` bigint(20) DEFAULT NULL;

ALTER TABLE `log`.`money_log`
  MODIFY COLUMN `gold` bigint(20) DEFAULT NULL;

ALTER TABLE `log`.`item_meta_log`
  MODIFY COLUMN `itemid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `who` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`exchange_log`
  MODIFY COLUMN `pid1` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `pid2` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `item_id` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `count` bigint(20) DEFAULT NULL;

ALTER TABLE `log`.`request_info_log`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`goldlog`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `what` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`cube`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `item_uid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`speed_hack`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`command_log`
  MODIFY COLUMN `userid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`refinelog`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `item_id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`dragon_slay_log`
  MODIFY COLUMN `guild_id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`chat_log`
  MODIFY COLUMN `who_id` bigint(20) unsigned NOT NULL,
  MODIFY COLUMN `whom_id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`captcha_log`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `invoker_pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`voucher_code_log`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `account_id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`report_player_log`
  MODIFY COLUMN `who_pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `target_pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`fish_log`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `log`.`acce`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `item_uid` bigint(20) unsigned DEFAULT NULL;

-- 4. Player Schema: 64-bit Persistent IDs, Timestamps, Economy (YANG), Affect Durations/Costs, Special Flags & ItemAward Uniqueness
ALTER TABLE `player`.`affect`
  MODIFY COLUMN `dwPID` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `lDuration` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `lSPCost` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`change_empire`
  MODIFY COLUMN `aid` bigint(20) unsigned NOT NULL;

ALTER TABLE `player`.`guild`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `master` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `token` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild_comment`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `guild_id` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `player`.`guild_grade`
  MODIFY COLUMN `guild_id` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild_land`
  MODIFY COLUMN `land_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `guild_id` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild_member`
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `guild_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `offer` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `player`.`guild_war`
  MODIFY COLUMN `id_from` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `id_to` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild_war_bet`
  MODIFY COLUMN `gold` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `guild` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `war_id` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`guild_war_reservation`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `guild1` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `guild2` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `warprice` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `winner` bigint(20) NOT NULL DEFAULT -1;

ALTER TABLE `player`.`horse_name`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`ikashop_auction_offer`
  MODIFY COLUMN `seller` bigint(20) unsigned NOT NULL,
  MODIFY COLUMN `buyer` bigint(20) unsigned NOT NULL;

ALTER TABLE `player`.`ikashop_notification`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `owner` bigint(20) unsigned NOT NULL;

ALTER TABLE `player`.`ikashop_offlineshop`
  MODIFY COLUMN `owner` bigint(20) unsigned NOT NULL;

ALTER TABLE `player`.`ikashop_private_offer`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `seller` bigint(20) unsigned NOT NULL,
  MODIFY COLUMN `buyer` bigint(20) unsigned NOT NULL,
  MODIFY COLUMN `item` bigint(20) unsigned NOT NULL;

ALTER TABLE `player`.`ikashop_safebox`
  MODIFY COLUMN `owner` bigint(20) unsigned NOT NULL;

ALTER TABLE `player`.`ikashop_sale`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL;

ALTER TABLE `player`.`item`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `owner_id` bigint(20) unsigned NOT NULL DEFAULT 0;

UPDATE `player`.`item_award`
  SET `why` = CONCAT(LEFT(`why`, 96), '#LEGACY_', `id`)
  WHERE `why` IS NOT NULL AND `why` NOT LIKE 'ITEMSHOP_TX_%' AND `why` NOT LIKE '%#LEGACY_%';

ALTER TABLE `player`.`item_award`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `item_id` bigint(20) unsigned DEFAULT NULL,
  ADD UNIQUE KEY IF NOT EXISTS `idx_item_award_why` (`why`) USING BTREE;

ALTER TABLE `player`.`lotto_list`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL;

ALTER TABLE `player`.`marriage`
  MODIFY COLUMN `pid1` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `pid2` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `time` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`messenger_block_list`
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL;

ALTER TABLE `player`.`monarch`
  MODIFY COLUMN `pid` bigint(20) unsigned DEFAULT NULL,
  MODIFY COLUMN `money` bigint(20) DEFAULT NULL;

ALTER TABLE `player`.`monarch_candidacy`
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`monarch_election`
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `selectedpid` bigint(20) unsigned DEFAULT 0;

ALTER TABLE `player`.`myshop_pricelist`
  MODIFY COLUMN `owner_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `price` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`object`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `land_id` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`pcbang_ip`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT;

ALTER TABLE `player`.`player`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN `account_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `horse_hp_droptime` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`player_deleted`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL,
  MODIFY COLUMN `account_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0,
  MODIFY COLUMN `horse_hp_droptime` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`player_index`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `pid1` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `pid2` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `pid3` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `pid4` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `pid5` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`player_special_flag`
  MODIFY COLUMN `pid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `aid` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `value` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`quest`
  MODIFY COLUMN `dwPID` bigint(20) unsigned NOT NULL DEFAULT 0;

ALTER TABLE `player`.`safebox`
  MODIFY COLUMN `account_id` bigint(20) unsigned NOT NULL DEFAULT 0,
  MODIFY COLUMN `gold` bigint(20) NOT NULL DEFAULT 0;

ALTER TABLE `player`.`sms_pool`
  MODIFY COLUMN `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT;

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
