/*
 * Project: curve
 * File Created: 2019-09-29
 * Author: charisu
 * Copyright (c)￼ 2018 netease
 */

#include <gtest/gtest.h>
#include "src/tools/namespace_tool.h"
#include "test/tools/mock_namespace_tool_core.h"

using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgPointee;

uint64_t segmentSize = 1 * 1024 * 1024 * 1024ul;   // NOLINT
uint64_t chunkSize = 16 * 1024 * 1024;   // NOLINT

DECLARE_bool(isTest);
DECLARE_string(fileName);
DECLARE_uint64(offset);
DEFINE_uint64(rpcTimeout, 3000, "millisecond for rpc timeout");
DEFINE_uint64(rpcRetryTimes, 5, "rpc retry times");

class NameSpaceToolTest : public ::testing::Test {
 protected:
    NameSpaceToolTest() {
        FLAGS_isTest = true;
    }
    void SetUp() {
        core_ = std::make_shared<curve::tool::MockNameSpaceToolCore>();
    }
    void TearDown() {
        core_ = nullptr;
    }

    void GetFileInfoForTest(FileInfo* fileInfo) {
        fileInfo->set_id(1);
        fileInfo->set_filename("test");
        fileInfo->set_parentid(0);
        fileInfo->set_filetype(curve::mds::FileType::INODE_PAGEFILE);
        fileInfo->set_segmentsize(segmentSize);
        fileInfo->set_length(5 * segmentSize);
        fileInfo->set_originalfullpathname("/cinder/test");
        fileInfo->set_ctime(1573546993000000);
    }

    void GetCsLocForTest(ChunkServerLocation* csLoc, uint64_t csId) {
        csLoc->set_chunkserverid(csId);
        csLoc->set_hostip("127.0.0.1");
        csLoc->set_port(9191 + csId);
    }

    void GetSegmentForTest(PageFileSegment* segment) {
        segment->set_logicalpoolid(1);
        segment->set_segmentsize(segmentSize);
        segment->set_chunksize(chunkSize);
        segment->set_startoffset(0);
        for (int i = 0; i < 10; ++i) {
            auto chunk = segment->add_chunks();
            chunk->set_copysetid(1000 + i);
            chunk->set_chunkid(2000 + i);
        }
    }
    std::shared_ptr<curve::tool::MockNameSpaceToolCore> core_;
};

TEST_F(NameSpaceToolTest, GetFile) {
    curve::tool::NameSpaceTool namespaceTool(core_);
    namespaceTool.PrintHelp("abc");
    ASSERT_EQ(-1, namespaceTool.RunCommand("abc"));
    namespaceTool.PrintHelp("get");
    FileInfo fileInfo;
    GetFileInfoForTest(&fileInfo);
    PageFileSegment segment;
    GetSegmentForTest(&segment);
    FLAGS_fileName = "/test/";

    // 1、正常情况
    EXPECT_CALL(*core_, GetFileInfo(_, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<1>(fileInfo),
                        Return(0)));
    EXPECT_CALL(*core_, GetAllocatedSize(_, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<1>(10 * segmentSize),
                        Return(0)));
    ASSERT_EQ(0, namespaceTool.RunCommand("get"));

    // 2、获取fileInfo失败
    EXPECT_CALL(*core_, GetFileInfo(_, _))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("get"));

    // 3、计算大小失败
     EXPECT_CALL(*core_, GetFileInfo(_, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<1>(fileInfo),
                        Return(0)));
    EXPECT_CALL(*core_, GetAllocatedSize(_, _))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("get"));
}

TEST_F(NameSpaceToolTest, ListDir) {
    curve::tool::NameSpaceTool namespaceTool(core_);
    namespaceTool.PrintHelp("list");
    FileInfo fileInfo;
    GetFileInfoForTest(&fileInfo);
    PageFileSegment segment;
    GetSegmentForTest(&segment);

    // 1、正常情况
    std::vector<FileInfo> files;
    for (uint64_t i = 0; i < 3; ++i) {
        files.emplace_back(fileInfo);
    }
    EXPECT_CALL(*core_, ListDir(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgPointee<1>(files),
                        Return(0)));
    EXPECT_CALL(*core_, GetAllocatedSize(_, _))
        .Times(6)
        .WillRepeatedly(DoAll(SetArgPointee<1>(10 * segmentSize),
                        Return(0)));
    FLAGS_fileName = "/";
    ASSERT_EQ(0, namespaceTool.RunCommand("list"));
    FLAGS_fileName = "/test/";
    ASSERT_EQ(0, namespaceTool.RunCommand("list"));

    // 2、listDir失败
    EXPECT_CALL(*core_, ListDir(_, _))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("list"));

    // 3、计算大小失败,个别的文件计算大小失败不应该返回错误
    EXPECT_CALL(*core_, ListDir(_, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<1>(files),
                        Return(0)));
    EXPECT_CALL(*core_, GetAllocatedSize(_, _))
        .Times(3)
        .WillOnce(Return(-1))
        .WillRepeatedly(DoAll(SetArgPointee<1>(10 * segmentSize),
                        Return(0)));
    ASSERT_EQ(0, namespaceTool.RunCommand("list"));
}

TEST_F(NameSpaceToolTest, SegInfo) {
    curve::tool::NameSpaceTool namespaceTool(core_);
    namespaceTool.PrintHelp("seginfo");
    std::vector<PageFileSegment> segments;
    for (int i = 0; i < 3; ++i) {
        PageFileSegment segment;
        GetSegmentForTest(&segment);
        segments.emplace_back(segment);
    }
    FLAGS_fileName = "/test";

    // 1、正常情况
    EXPECT_CALL(*core_, GetFileSegments(_, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<1>(segments),
                        Return(0)));
    ASSERT_EQ(0, namespaceTool.RunCommand("seginfo"));

    // 2、GetFileSegment失败
    EXPECT_CALL(*core_, GetFileSegments(_, _))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("seginfo"));
}

TEST_F(NameSpaceToolTest, CreateFile) {
    curve::tool::NameSpaceTool namespaceTool(core_);
    namespaceTool.PrintHelp("create");

    // 1、正常情况
    EXPECT_CALL(*core_, CreateFile(_, _))
        .Times(1)
        .WillOnce(Return(0));
    ASSERT_EQ(0, namespaceTool.RunCommand("create"));

    // 2、创建失败
    EXPECT_CALL(*core_, CreateFile(_, _))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("create"));
}

TEST_F(NameSpaceToolTest, DeleteFile) {
    curve::tool::NameSpaceTool namespaceTool(core_);
    namespaceTool.PrintHelp("delete");

    // 1、正常情况
    EXPECT_CALL(*core_, DeleteFile(_, _))
        .Times(1)
        .WillOnce(Return(0));
    ASSERT_EQ(0, namespaceTool.RunCommand("delete"));

    // 2、创建失败
    EXPECT_CALL(*core_, DeleteFile(_, _))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("delete"));
}

TEST_F(NameSpaceToolTest, CleanRecycle) {
    curve::tool::NameSpaceTool namespaceTool(core_);
    namespaceTool.PrintHelp("clean-recycle");

    // 1、正常情况
    EXPECT_CALL(*core_, CleanRecycleBin(_))
        .Times(1)
        .WillOnce(Return(0));
    ASSERT_EQ(0, namespaceTool.RunCommand("clean-recycle"));

    // 2、失败
    EXPECT_CALL(*core_, CleanRecycleBin(_))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("clean-recycle"));
}

TEST_F(NameSpaceToolTest, PrintChunkLocation) {
    curve::tool::NameSpaceTool namespaceTool(core_);
    namespaceTool.PrintHelp("chunk-location");
    std::vector<ChunkServerLocation> csLocs;
    for (uint64_t i = 0; i < 3; ++i) {
        ChunkServerLocation csLoc;
        GetCsLocForTest(&csLoc, i);
        csLocs.emplace_back(csLoc);
    }
    uint64_t chunkId = 2001;
    std::pair<uint32_t, uint32_t> copyset = {1, 101};

    // 1、正常情况
    EXPECT_CALL(*core_, QueryChunkCopyset(_, _, _, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<2>(chunkId),
                        SetArgPointee<3>(copyset),
                        Return(0)));
    EXPECT_CALL(*core_, GetChunkServerListInCopySets(_, _, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<2>(csLocs),
                        Return(0)));
    ASSERT_EQ(0, namespaceTool.RunCommand("chunk-location"));

    // 2、QueryChunkCopyset失败
    EXPECT_CALL(*core_, QueryChunkCopyset(_, _, _, _))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("chunk-location"));

    // 3、GetChunkServerListInCopySets失败
    EXPECT_CALL(*core_, QueryChunkCopyset(_, _, _, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<2>(chunkId),
                        SetArgPointee<3>(copyset),
                        Return(0)));
    EXPECT_CALL(*core_, GetChunkServerListInCopySets(_, _, _))
        .Times(1)
        .WillOnce(Return(-1));
    ASSERT_EQ(-1, namespaceTool.RunCommand("chunk-location"));
}